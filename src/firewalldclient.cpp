/*
 * tde-firewalld - firewalld manager for the Trinity Desktop Environment
 *
 * Copyright (C) 2026 tde-firewalld developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "firewalldclient.h"

#include <tqdir.h>
#include <tqfile.h>
#include <tqfileinfo.h>
#include <tqtimer.h>

#include <tqdbusdatalist.h>
#include <tqdbusdatamap.h>
#include <tqdbuserror.h>
#include <tqdbusobjectpath.h>
#include <tqdbusvariant.h>

#include <tdelocale.h>
#include <kdebug.h>

#include <dbus/dbus.h>

#include <string.h>

/* firewalld asks polkit with a 60 s timeout; the user may need all of it to
 * type the password, so wait a little longer than that. */
static const int CALL_TIMEOUT = 125 * 1000;

static const char *FD_SERVICE_BUS   = "org.freedesktop.DBus";
static const char *FD_PATH_BUS      = "/org/freedesktop/DBus";
static const char *FD_INTERFACE_BUS = "org.freedesktop.DBus";
static const char *FD_INTERFACE_PROPERTIES = "org.freedesktop.DBus.Properties";

/* ---------------------------------------------------------------------- */
/* D-Bus data conversion helpers                                          */
/* ---------------------------------------------------------------------- */

static TQT_DBusData unwrap(const TQT_DBusData &data)
{
	if (data.type() == TQT_DBusData::Variant)
		return data.toVariant().value;
	return data;
}

static TQString toStr(const TQT_DBusData &data)
{
	return unwrap(data).toString();
}

static bool toBool(const TQT_DBusData &data)
{
	return unwrap(data).toBool();
}

static TQStringList toStrList(const TQT_DBusData &data)
{
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::List)
		return TQStringList();
	return d.toList().toTQStringList();
}

/* a(ss), a(ssss), ... -> list of string lists */
static TQValueList<TQStringList> toStructList(const TQT_DBusData &data)
{
	TQValueList<TQStringList> result;
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::List)
		return result;
	TQValueList<TQT_DBusData> items = d.toList().toTQValueList();
	TQValueList<TQT_DBusData>::const_iterator it;
	for (it = items.begin(); it != items.end(); ++it) {
		TQStringList fields;
		TQT_DBusData item = *it;
		TQValueList<TQT_DBusData> members;
		if (item.type() == TQT_DBusData::Struct)
			members = item.toStruct();
		else if (item.type() == TQT_DBusData::List)
			members = item.toList().toTQValueList();
		TQValueList<TQT_DBusData>::const_iterator m;
		for (m = members.begin(); m != members.end(); ++m)
			fields << unwrap(*m).toString();
		result << fields;
	}
	return result;
}

static FwPortList toPortList(const TQT_DBusData &data)
{
	FwPortList ports;
	TQValueList<TQStringList> list = toStructList(data);
	TQValueList<TQStringList>::const_iterator it;
	for (it = list.begin(); it != list.end(); ++it) {
		if ((*it).count() < 2)
			continue;
		FwPort p;
		p.port = (*it)[0];
		p.protocol = (*it)[1];
		ports << p;
	}
	return ports;
}

typedef TQMap<TQString, TQT_DBusData> VariantMap;

static VariantMap toVariantMap(const TQT_DBusData &data)
{
	VariantMap map;
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::Map)
		return map;
	TQT_DBusDataMap<TQString> dm = d.toStringKeyMap();
	TQT_DBusDataMap<TQString>::const_iterator it;
	for (it = dm.begin(); it != dm.end(); ++it)
		map[it.key()] = unwrap(it.data());
	return map;
}

static TQValueList<TQT_DBusData> args(const TQString &a)
{
	TQValueList<TQT_DBusData> l;
	l << TQT_DBusData::fromString(a);
	return l;
}

static TQValueList<TQT_DBusData> args(const TQString &a, const TQString &b)
{
	TQValueList<TQT_DBusData> l = args(a);
	l << TQT_DBusData::fromString(b);
	return l;
}

/* ---------------------------------------------------------------------- */

FirewalldClient::FirewalldClient(TQObject *parent, const char *name)
	: TQObject(parent, name), m_mode(Runtime), m_running(false), m_authDenied(false)
{
	m_bus = TQT_DBusConnection::systemBus();

	m_changeTimer = new TQTimer(this);
	connect(m_changeTimer, TQ_SIGNAL(timeout()), this, TQ_SLOT(slotEmitChanged()));

	if (m_bus.isConnected()) {
		m_bus.connect(this, TQ_SLOT(slotDBusSignal(const TQT_DBusMessage&)));
		addMatchRules();
		m_running = isRunning();
	} else {
		m_lastError = i18n("Cannot connect to the D-Bus system bus.");
		kdWarning() << "tde-firewalld: no system bus connection" << endl;
	}
}

FirewalldClient::~FirewalldClient()
{
}

void FirewalldClient::addMatchRules()
{
	TQStringList rules;
	rules << TQString("type='signal',sender='%1'").arg(FW_DBUS_SERVICE);
	rules << TQString("type='signal',sender='%1',interface='%2',member='NameOwnerChanged',arg0='%3'")
	         .arg(FD_SERVICE_BUS).arg(FD_INTERFACE_BUS).arg(FW_DBUS_SERVICE);

	for (TQStringList::const_iterator it = rules.begin(); it != rules.end(); ++it) {
		TQT_DBusMessage msg = TQT_DBusMessage::methodCall(FD_SERVICE_BUS, FD_PATH_BUS,
		                                                  FD_INTERFACE_BUS, "AddMatch");
		msg << TQT_DBusData::fromString(*it);
		m_bus.send(msg);
	}
}

bool FirewalldClient::busConnected() const
{
	return m_bus.isConnected();
}

/* ---------------------------------------------------------------------- */
/* low level calls                                                        */
/* ---------------------------------------------------------------------- */

TQT_DBusMessage FirewalldClient::call(const TQString &path, const TQString &interface,
                                      const TQString &method,
                                      const TQValueList<TQT_DBusData> &arguments)
{
	bool toFirewalld = !path.startsWith(FD_PATH_BUS);
	TQString service = toFirewalld ? TQString(FW_DBUS_SERVICE) : TQString(FD_SERVICE_BUS);

	/* methods that only need the "info" polkit action (allowed for every
	 * user by default) are always sent, see firewalld's server/firewalld.py.
	 * Methods without an explicit polkit decorator (D-Bus properties, the
	 * permanent configuration getters) need the "config" action. */
	static const char *infoMethods[] = {
		"getZones", "getActiveZones", "getDefaultZone", "listServices",
		"getZoneOfInterface", "queryPanicMode", 0
	};
	bool infoOnly = false;
	for (int i = 0; infoMethods[i]; ++i)
		if (method == infoMethods[i])
			infoOnly = true;

	if (toFirewalld && m_authDenied && !infoOnly) {
		m_lastErrorName = "org.fedoraproject.FirewallD1.NotAuthorizedException";
		m_lastError = friendlyError(m_lastErrorName, TQString::null);
		return TQT_DBusMessage();
	}
	TQT_DBusMessage msg = TQT_DBusMessage::methodCall(service, path, interface, method);
	for (TQValueList<TQT_DBusData>::const_iterator it = arguments.begin(); it != arguments.end(); ++it)
		msg << *it;
	msg.setTimeout(CALL_TIMEOUT);

	TQT_DBusError error;
	TQT_DBusMessage reply = m_bus.sendWithReply(msg, &error);

	if (reply.type() == TQT_DBusMessage::ErrorMessage) {
		m_lastErrorName = reply.error().name();
		m_lastError = friendlyError(reply.error().name(), reply.error().message());
	} else if (error.isValid()) {
		m_lastErrorName = error.name();
		m_lastError = friendlyError(error.name(), error.message());
	} else {
		m_lastErrorName = TQString::null;
		m_lastError = TQString::null;
	}
	if (!m_lastErrorName.isEmpty())
		kdDebug() << "tde-firewalld: " << method << " failed: " << m_lastErrorName << endl;
	if (toFirewalld && m_lastErrorName.endsWith(".NotAuthorizedException")) {
		m_authDenied = true;
		emit authorizationRequired();
	}
	return reply;
}

bool FirewalldClient::callAsync(const TQString &path, const TQString &interface,
                                const TQString &method, const TQValueList<TQT_DBusData> &arguments,
                                const TQString &description)
{
	TQT_DBusMessage msg = TQT_DBusMessage::methodCall(FW_DBUS_SERVICE, path, interface, method);
	for (TQValueList<TQT_DBusData>::const_iterator it = arguments.begin(); it != arguments.end(); ++it)
		msg << *it;
	msg.setTimeout(CALL_TIMEOUT);

	int id = m_bus.sendWithAsyncReply(msg, this, TQ_SLOT(slotAsyncReply(const TQT_DBusMessage&)));
	if (id == 0) {
		TQString err = friendlyError(m_bus.lastError().name(), m_bus.lastError().message());
		emit operationFinished(false, description, err);
		return false;
	}
	m_pending[id] = description;
	return true;
}

void FirewalldClient::slotAsyncReply(const TQT_DBusMessage &reply)
{
	int id = reply.replySerialNumber();
	TQString description;
	if (m_pending.contains(id)) {
		description = m_pending[id];
		m_pending.remove(id);
	}

	if (reply.type() == TQT_DBusMessage::ErrorMessage) {
		TQT_DBusError err = reply.error();
		emit operationFinished(false, description, friendlyError(err.name(), err.message()));
	} else {
		emit operationFinished(true, description, TQString::null);
	}
	/* not every request is followed by a firewalld signal (e.g. permanent
	 * changes before a reload), so always refresh */
	m_changeTimer->start(150, true);
}

void FirewalldClient::slotDBusSignal(const TQT_DBusMessage &message)
{
	if (message.member() == "NameOwnerChanged" && message.interface() == FD_INTERFACE_BUS) {
		if (message.count() >= 3 && message[0].toString() == FW_DBUS_SERVICE) {
			bool running = !message[2].toString().isEmpty();
			if (running != m_running) {
				m_running = running;
				emit availabilityChanged(running);
			}
		}
		return;
	}
	if (message.interface().startsWith(FW_DBUS_INTERFACE))
		m_changeTimer->start(300, true);
}

void FirewalldClient::slotEmitChanged()
{
	emit configurationChanged();
}

TQStringList FirewalldClient::stringListCall(const TQString &path, const TQString &interface,
                                            const TQString &method,
                                            const TQValueList<TQT_DBusData> &arguments)
{
	TQT_DBusMessage reply = call(path, interface, method, arguments);
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQStringList();
	/* sort case-insensitively */
	TQStringList list = toStrList(reply[0]);
	TQMap<TQString, TQString> sorted;
	for (TQStringList::const_iterator it = list.begin(); it != list.end(); ++it)
		sorted[(*it).lower() + TQChar(0) + *it] = *it;
	return sorted.values();
}

TQString FirewalldClient::configZonePath(const TQString &zone)
{
	TQT_DBusMessage reply = call(FW_DBUS_PATH_CONFIG, FW_DBUS_INTERFACE_CONFIG,
	                             "getZoneByName", args(zone));
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQString::null;
	return reply[0].toObjectPath();
}

TQString FirewalldClient::configServicePath(const TQString &service)
{
	TQT_DBusMessage reply = call(FW_DBUS_PATH_CONFIG, FW_DBUS_INTERFACE_CONFIG,
	                             "getServiceByName", args(service));
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQString::null;
	return reply[0].toObjectPath();
}

/* ---------------------------------------------------------------------- */
/* state                                                                  */
/* ---------------------------------------------------------------------- */

bool FirewalldClient::isRunning()
{
	if (!m_bus.isConnected())
		return false;
	TQT_DBusMessage reply = call(FD_PATH_BUS, FD_INTERFACE_BUS, "NameHasOwner",
	                             args(FW_DBUS_SERVICE));
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return false;
	m_running = reply[0].toBool();
	return m_running;
}

TQString FirewalldClient::version()
{
	TQT_DBusMessage reply = call(FW_DBUS_PATH, FD_INTERFACE_PROPERTIES, "Get",
	                             args(FW_DBUS_INTERFACE, "version"));
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQString::null;
	return toStr(reply[0]);
}

TQString FirewalldClient::state()
{
	TQT_DBusMessage reply = call(FW_DBUS_PATH, FD_INTERFACE_PROPERTIES, "Get",
	                             args(FW_DBUS_INTERFACE, "state"));
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQString::null;
	return toStr(reply[0]);
}

void FirewalldClient::setMode(Mode mode)
{
	m_mode = mode;
}

bool FirewalldClient::polkitAgentRunning()
{
	static const char *agents[] = {
		"polkit-agent-tde",
		"polkit-kde-authentication-agent-1",
		"polkit-gnome-authentication-agent-1",
		"polkit-mate-authentication-agent-1",
		"lxpolkit",
		"lxqt-policykit-agent",
		"xfce-polkit",
		"mate-polkit",
		"polkit-1-auth-agent",
		0
	};

	TQDir proc("/proc");
	TQStringList pids = proc.entryList(TQDir::Dirs);
	for (TQStringList::const_iterator it = pids.begin(); it != pids.end(); ++it) {
		bool isPid = false;
		(*it).toInt(&isPid);
		if (!isPid)
			continue;
		TQFile f(TQString("/proc/%1/cmdline").arg(*it));
		if (!f.open(IO_ReadOnly))
			continue;
		TQByteArray raw = f.readAll();
		f.close();
		/* the first NUL terminated string is argv[0] */
		TQString cmd = TQString::fromLocal8Bit(raw.data(), strnlen(raw.data(), raw.size()));
		cmd = cmd.section('/', -1);
		for (int i = 0; agents[i]; ++i) {
			if (cmd == agents[i])
				return true;
		}
	}
	return false;
}

/* ---------------------------------------------------------------------- */
/* reading                                                                */
/* ---------------------------------------------------------------------- */

TQStringList FirewalldClient::zones()
{
	if (m_mode == Permanent)
		return stringListCall(FW_DBUS_PATH_CONFIG, FW_DBUS_INTERFACE_CONFIG, "getZoneNames");
	return stringListCall(FW_DBUS_PATH, FW_DBUS_INTERFACE_ZONE, "getZones");
}

FwZone FirewalldClient::zone(const TQString &name)
{
	FwZone z;
	z.name = name;

	TQT_DBusMessage reply;
	if (m_mode == Permanent) {
		TQString path = configZonePath(name);
		if (path.isEmpty())
			return z;
		reply = call(path, FW_DBUS_INTERFACE_CONFIG_ZONE, "getSettings2");
	} else {
		reply = call(FW_DBUS_PATH, FW_DBUS_INTERFACE_ZONE, "getZoneSettings2", args(name));
	}
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return z;

	VariantMap s = toVariantMap(reply[0]);
	z.shortName   = toStr(s["short"]);
	z.description = toStr(s["description"]);
	z.target      = s.contains("target") ? toStr(s["target"]) : TQString("default");
	z.services    = toStrList(s["services"]);
	z.ports       = toPortList(s["ports"]);
	z.protocols   = toStrList(s["protocols"]);
	z.sourcePorts = toPortList(s["source_ports"]);
	z.icmpBlocks  = toStrList(s["icmp_blocks"]);
	z.interfaces  = toStrList(s["interfaces"]);
	z.sources     = toStrList(s["sources"]);
	z.richRules   = toStrList(s["rules_str"]);
	z.masquerade  = s.contains("masquerade") && toBool(s["masquerade"]);
	z.icmpBlockInversion = s.contains("icmp_block_inversion") && toBool(s["icmp_block_inversion"]);
	z.forward     = s.contains("forward") && toBool(s["forward"]);

	TQValueList<TQStringList> fwd = toStructList(s["forward_ports"]);
	for (TQValueList<TQStringList>::const_iterator it = fwd.begin(); it != fwd.end(); ++it) {
		if ((*it).count() < 4)
			continue;
		FwForwardPort fp;
		fp.port = (*it)[0];
		fp.protocol = (*it)[1];
		fp.toPort = (*it)[2];
		fp.toAddr = (*it)[3];
		z.forwardPorts << fp;
	}

	z.services.sort();
	z.valid = true;
	return z;
}

TQMap<TQString, TQStringList> FirewalldClient::activeZoneEntries(const TQString &key)
{
	TQMap<TQString, TQStringList> result;
	TQT_DBusMessage reply = call(FW_DBUS_PATH, FW_DBUS_INTERFACE_ZONE, "getActiveZones");
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return result;

	/* a{sa{sas}} */
	VariantMap zones = toVariantMap(reply[0]);
	for (VariantMap::const_iterator it = zones.begin(); it != zones.end(); ++it) {
		VariantMap entry = toVariantMap(it.data());
		if (entry.contains(key))
			result[it.key()] = toStrList(entry[key]);
		else
			result[it.key()] = TQStringList();
	}
	return result;
}

TQMap<TQString, TQStringList> FirewalldClient::activeZoneInterfaces()
{
	return activeZoneEntries("interfaces");
}

TQMap<TQString, TQStringList> FirewalldClient::activeZoneSources()
{
	return activeZoneEntries("sources");
}

TQString FirewalldClient::zoneOfInterface(const TQString &iface)
{
	TQT_DBusMessage reply;
	if (m_mode == Permanent)
		reply = call(FW_DBUS_PATH_CONFIG, FW_DBUS_INTERFACE_CONFIG, "getZoneOfInterface", args(iface));
	else
		reply = call(FW_DBUS_PATH, FW_DBUS_INTERFACE_ZONE, "getZoneOfInterface", args(iface));
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQString::null;
	return reply[0].toString();
}

TQStringList FirewalldClient::services()
{
	if (m_mode == Permanent)
		return stringListCall(FW_DBUS_PATH_CONFIG, FW_DBUS_INTERFACE_CONFIG, "getServiceNames");
	return stringListCall(FW_DBUS_PATH, FW_DBUS_INTERFACE, "listServices");
}

FwService FirewalldClient::service(const TQString &name)
{
	FwService svc;
	svc.name = name;

	TQT_DBusMessage reply;
	if (m_mode == Permanent) {
		TQString path = configServicePath(name);
		if (path.isEmpty())
			return svc;
		reply = call(path, FW_DBUS_INTERFACE_CONFIG_SERVICE, "getSettings2");
	} else {
		reply = call(FW_DBUS_PATH, FW_DBUS_INTERFACE, "getServiceSettings2", args(name));
	}
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return svc;

	VariantMap s = toVariantMap(reply[0]);
	svc.shortName   = toStr(s["short"]);
	svc.description = toStr(s["description"]);
	svc.ports       = toPortList(s["ports"]);
	svc.protocols   = toStrList(s["protocols"]);
	svc.sourcePorts = toPortList(s["source_ports"]);
	svc.modules     = toStrList(s["module_names"]);
	svc.includes    = toStrList(s["includes"]);
	if (s.contains("helpers"))
		svc.modules += toStrList(s["helpers"]);

	VariantMap dest = toVariantMap(s["destination"]);
	for (VariantMap::const_iterator it = dest.begin(); it != dest.end(); ++it)
		svc.destinations[it.key()] = toStr(it.data());

	svc.valid = true;
	return svc;
}

TQString FirewalldClient::defaultZone()
{
	TQT_DBusMessage reply = call(FW_DBUS_PATH, FW_DBUS_INTERFACE, "getDefaultZone");
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQString::null;
	return reply[0].toString();
}

bool FirewalldClient::panicMode()
{
	TQT_DBusMessage reply = call(FW_DBUS_PATH, FW_DBUS_INTERFACE, "queryPanicMode");
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return false;
	return reply[0].toBool();
}

TQString FirewalldClient::logDenied()
{
	TQT_DBusMessage reply = call(FW_DBUS_PATH, FW_DBUS_INTERFACE, "getLogDenied");
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQString::null;
	return reply[0].toString();
}

TQStringList FirewalldClient::systemInterfaces()
{
	TQDir dir("/sys/class/net");
	TQStringList result;
	TQStringList entries = dir.entryList(TQDir::All | TQDir::System);
	for (TQStringList::const_iterator it = entries.begin(); it != entries.end(); ++it) {
		if (*it == "." || *it == ".." || *it == "lo" || *it == "bonding_masters")
			continue;
		result << *it;
	}
	result.sort();
	return result;
}

/* ---------------------------------------------------------------------- */
/* writing                                                                */
/* ---------------------------------------------------------------------- */

void FirewalldClient::zoneCall(const TQString &zone, const TQString &method,
                               const TQValueList<TQT_DBusData> &arguments, bool runtimeTimeout,
                               const TQString &description)
{
	if (m_mode == Permanent) {
		TQString path = configZonePath(zone);
		if (path.isEmpty()) {
			emit operationFinished(false, description, m_lastError);
			return;
		}
		callAsync(path, FW_DBUS_INTERFACE_CONFIG_ZONE, method, arguments, description);
		return;
	}

	TQValueList<TQT_DBusData> a;
	a << TQT_DBusData::fromString(zone);
	a += arguments;
	if (runtimeTimeout)
		a << TQT_DBusData::fromInt32(0);
	callAsync(FW_DBUS_PATH, FW_DBUS_INTERFACE_ZONE, method, a, description);
}

void FirewalldClient::addService(const TQString &zone, const TQString &service)
{
	zoneCall(zone, "addService", args(service), true,
	         i18n("Allow service %1 in zone %2").arg(service).arg(zone));
}

void FirewalldClient::removeService(const TQString &zone, const TQString &service)
{
	zoneCall(zone, "removeService", args(service), false,
	         i18n("Remove service %1 from zone %2").arg(service).arg(zone));
}

void FirewalldClient::addPort(const TQString &zone, const TQString &port, const TQString &proto)
{
	zoneCall(zone, "addPort", args(port, proto), true,
	         i18n("Open port %1/%2 in zone %3").arg(port).arg(proto).arg(zone));
}

void FirewalldClient::removePort(const TQString &zone, const TQString &port, const TQString &proto)
{
	zoneCall(zone, "removePort", args(port, proto), false,
	         i18n("Close port %1/%2 in zone %3").arg(port).arg(proto).arg(zone));
}

void FirewalldClient::setMasquerade(const TQString &zone, bool enable)
{
	if (enable)
		zoneCall(zone, "addMasquerade", TQValueList<TQT_DBusData>(), true,
		         i18n("Enable masquerading in zone %1").arg(zone));
	else
		zoneCall(zone, "removeMasquerade", TQValueList<TQT_DBusData>(), false,
		         i18n("Disable masquerading in zone %1").arg(zone));
}

static TQValueList<TQT_DBusData> forwardArgs(const FwForwardPort &fp)
{
	TQValueList<TQT_DBusData> a = args(fp.port, fp.protocol);
	a << TQT_DBusData::fromString(fp.toPort);
	a << TQT_DBusData::fromString(fp.toAddr);
	return a;
}

void FirewalldClient::addForwardPort(const TQString &zone, const FwForwardPort &fp)
{
	zoneCall(zone, "addForwardPort", forwardArgs(fp), true,
	         i18n("Add port forwarding %1/%2 in zone %3").arg(fp.port).arg(fp.protocol).arg(zone));
}

void FirewalldClient::removeForwardPort(const TQString &zone, const FwForwardPort &fp)
{
	zoneCall(zone, "removeForwardPort", forwardArgs(fp), false,
	         i18n("Remove port forwarding %1/%2 from zone %3").arg(fp.port).arg(fp.protocol).arg(zone));
}

void FirewalldClient::addRichRule(const TQString &zone, const TQString &rule)
{
	zoneCall(zone, "addRichRule", args(rule), true,
	         i18n("Add rich rule to zone %1").arg(zone));
}

void FirewalldClient::removeRichRule(const TQString &zone, const TQString &rule)
{
	zoneCall(zone, "removeRichRule", args(rule), false,
	         i18n("Remove rich rule from zone %1").arg(zone));
}

void FirewalldClient::addInterface(const TQString &zone, const TQString &iface)
{
	zoneCall(zone, "addInterface", args(iface), false,
	         i18n("Bind interface %1 to zone %2").arg(iface).arg(zone));
}

void FirewalldClient::removeInterface(const TQString &zone, const TQString &iface)
{
	zoneCall(zone, "removeInterface", args(iface), false,
	         i18n("Remove interface %1 from zone %2").arg(iface).arg(zone));
}

void FirewalldClient::changeZoneOfInterface(const TQString &zone, const TQString &iface)
{
	TQString old = zoneOfInterface(iface);
	if (old == zone)
		return;

	if (m_mode == Runtime && !zone.isEmpty()) {
		callAsync(FW_DBUS_PATH, FW_DBUS_INTERFACE_ZONE, "changeZoneOfInterface", args(zone, iface),
		          i18n("Move interface %1 to zone %2").arg(iface).arg(zone));
		return;
	}

	/* permanent configuration, or "no zone" (use the default zone) */
	if (!old.isEmpty())
		removeInterface(old, iface);
	if (!zone.isEmpty())
		addInterface(zone, iface);
}

void FirewalldClient::addSource(const TQString &zone, const TQString &source)
{
	zoneCall(zone, "addSource", args(source), false,
	         i18n("Bind source %1 to zone %2").arg(source).arg(zone));
}

void FirewalldClient::removeSource(const TQString &zone, const TQString &source)
{
	zoneCall(zone, "removeSource", args(source), false,
	         i18n("Remove source %1 from zone %2").arg(source).arg(zone));
}

void FirewalldClient::setTarget(const TQString &zone, const TQString &target)
{
	TQString description = i18n("Set target of zone %1 to %2").arg(zone).arg(target);
	if (m_mode != Permanent) {
		emit operationFinished(false, description,
		                       i18n("The zone target can only be changed in the permanent configuration."));
		return;
	}
	zoneCall(zone, "setTarget", args(target), false, description);
}

void FirewalldClient::setDefaultZone(const TQString &zone)
{
	callAsync(FW_DBUS_PATH, FW_DBUS_INTERFACE, "setDefaultZone", args(zone),
	          i18n("Set default zone to %1").arg(zone));
}

void FirewalldClient::setPanicMode(bool enable)
{
	if (enable)
		callAsync(FW_DBUS_PATH, FW_DBUS_INTERFACE, "enablePanicMode", TQValueList<TQT_DBusData>(),
		          i18n("Enable panic mode"));
	else
		callAsync(FW_DBUS_PATH, FW_DBUS_INTERFACE, "disablePanicMode", TQValueList<TQT_DBusData>(),
		          i18n("Disable panic mode"));
}

void FirewalldClient::setLogDenied(const TQString &value)
{
	callAsync(FW_DBUS_PATH, FW_DBUS_INTERFACE, "setLogDenied", args(value),
	          i18n("Set logging of denied packets to %1").arg(value));
}

void FirewalldClient::reload()
{
	callAsync(FW_DBUS_PATH, FW_DBUS_INTERFACE, "reload", TQValueList<TQT_DBusData>(),
	          i18n("Reload firewalld"));
}

void FirewalldClient::runtimeToPermanent()
{
	callAsync(FW_DBUS_PATH, FW_DBUS_INTERFACE, "runtimeToPermanent", TQValueList<TQT_DBusData>(),
	          i18n("Make runtime configuration permanent"));
}

/* ---------------------------------------------------------------------- */

bool FirewalldClient::startFirewalld()
{
	/* systemd only shows a polkit prompt if the caller sets the
	 * "allow interactive authorization" flag, which dbus-1-tqt does not
	 * expose, so use libdbus directly for this single call. */
	DBusError err;
	dbus_error_init(&err);

	DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
	if (!conn) {
		m_lastError = TQString::fromUtf8(err.message ? err.message : "");
		dbus_error_free(&err);
		return false;
	}
	dbus_connection_set_exit_on_disconnect(conn, FALSE);

	DBusMessage *msg = dbus_message_new_method_call("org.freedesktop.systemd1",
	                                                "/org/freedesktop/systemd1",
	                                                "org.freedesktop.systemd1.Manager",
	                                                "StartUnit");
	const char *unit = "firewalld.service";
	const char *mode = "replace";
	dbus_message_append_args(msg, DBUS_TYPE_STRING, &unit, DBUS_TYPE_STRING, &mode, DBUS_TYPE_INVALID);
	dbus_message_set_allow_interactive_authorization(msg, TRUE);

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, CALL_TIMEOUT, &err);
	dbus_message_unref(msg);

	bool ok = (reply != 0);
	if (reply)
		dbus_message_unref(reply);
	if (dbus_error_is_set(&err)) {
		m_lastErrorName = TQString::fromUtf8(err.name);
		m_lastError = friendlyError(m_lastErrorName, TQString::fromUtf8(err.message));
		dbus_error_free(&err);
	}
	dbus_connection_close(conn);
	dbus_connection_unref(conn);
	return ok;
}

TQString FirewalldClient::friendlyError(const TQString &name, const TQString &message)
{
	if (name.endsWith(".NotAuthorizedException") ||
	    name == "org.freedesktop.DBus.Error.AccessDenied" ||
	    name == "org.freedesktop.DBus.Error.InteractiveAuthorizationRequired")
		return i18n("Not authorized. Authentication failed or was cancelled. "
		            "Make sure a polkit authentication agent such as polkit-agent-tde is running.");
	if (name == "org.freedesktop.DBus.Error.ServiceUnknown" ||
	    name == "org.freedesktop.DBus.Error.NameHasNoOwner")
		return i18n("The firewalld service is not running.");
	if (name == "org.freedesktop.DBus.Error.NoReply" ||
	    name == "org.freedesktop.DBus.Error.Timeout")
		return i18n("firewalld did not answer in time.");

	/* firewalld errors look like "INVALID_PORT: 99999" */
	TQString code = message.section(':', 0, 0).stripWhiteSpace();
	TQString detail = message.section(':', 1).stripWhiteSpace();
	if (code == "ALREADY_ENABLED")
		return i18n("Already enabled: %1").arg(detail);
	if (code == "NOT_ENABLED")
		return i18n("Not enabled: %1").arg(detail);
	if (code == "INVALID_PORT")
		return i18n("Invalid port: %1").arg(detail);
	if (code == "INVALID_PROTOCOL")
		return i18n("Invalid protocol: %1").arg(detail);
	if (code == "INVALID_RULE")
		return i18n("Invalid rich rule: %1").arg(detail);
	if (code == "INVALID_ADDR")
		return i18n("Invalid address: %1").arg(detail);
	if (code == "INVALID_ZONE")
		return i18n("Invalid zone: %1").arg(detail);
	if (code == "ZONE_CONFLICT")
		return i18n("Zone conflict: %1").arg(detail);
	if (code == "RUNNING_BUT_FAILED")
		return i18n("firewalld is running but failed to load its configuration: %1").arg(detail);

	if (message.isEmpty())
		return name;
	return message;
}

#include "firewalldclient.moc"
