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

#ifndef FIREWALLDCLIENT_H
#define FIREWALLDCLIENT_H

#include <tqobject.h>
#include <tqstring.h>
#include <tqstringlist.h>
#include <tqvaluelist.h>
#include <tqmap.h>

#include <tqdbusconnection.h>
#include <tqdbusdata.h>
#include <tqdbusmessage.h>

class TQTimer;

/* D-Bus names, see firewalld src/firewall/config/dbus.py */
#define FW_DBUS_SERVICE             "org.fedoraproject.FirewallD1"
#define FW_DBUS_PATH                "/org/fedoraproject/FirewallD1"
#define FW_DBUS_PATH_CONFIG         "/org/fedoraproject/FirewallD1/config"
#define FW_DBUS_INTERFACE           "org.fedoraproject.FirewallD1"
#define FW_DBUS_INTERFACE_ZONE      "org.fedoraproject.FirewallD1.zone"
#define FW_DBUS_INTERFACE_CONFIG    "org.fedoraproject.FirewallD1.config"
#define FW_DBUS_INTERFACE_CONFIG_ZONE    "org.fedoraproject.FirewallD1.config.zone"
#define FW_DBUS_INTERFACE_CONFIG_SERVICE "org.fedoraproject.FirewallD1.config.service"

struct FwPort
{
	TQString port;
	TQString protocol;
};
typedef TQValueList<FwPort> FwPortList;

struct FwForwardPort
{
	TQString port;
	TQString protocol;
	TQString toPort;
	TQString toAddr;
};
typedef TQValueList<FwForwardPort> FwForwardPortList;

struct FwZone
{
	FwZone() : masquerade(false), icmpBlockInversion(false), forward(false), valid(false) {}

	TQString name;
	TQString shortName;
	TQString description;
	TQString target;
	TQStringList services;
	FwPortList ports;
	TQStringList protocols;
	FwPortList sourcePorts;
	TQStringList icmpBlocks;
	FwForwardPortList forwardPorts;
	TQStringList interfaces;
	TQStringList sources;
	TQStringList richRules;
	bool masquerade;
	bool icmpBlockInversion;
	bool forward;
	bool valid;
};

struct FwService
{
	FwService() : valid(false) {}

	TQString name;
	TQString shortName;
	TQString description;
	FwPortList ports;
	TQStringList protocols;
	FwPortList sourcePorts;
	TQStringList modules;
	TQStringList includes;
	TQMap<TQString, TQString> destinations;
	bool valid;
};

/**
 * Thin client for the firewalld D-Bus API.
 *
 * All requests are sent to the firewalld daemon on the system bus.
 * firewalld checks every request with polkit (action ids
 * org.fedoraproject.FirewallD1.info / .config / .all) and allows user
 * interaction, so a running polkit agent (polkit-agent-tde) asks the user
 * for a password whenever authorisation is required.
 *
 * Read requests are synchronous with a long timeout (the user might be
 * typing a password), write requests are asynchronous and report their
 * result through operationFinished().
 */
class FirewalldClient : public TQObject
{
	TQ_OBJECT

public:
	enum Mode { Runtime = 0, Permanent = 1 };

	FirewalldClient(TQObject *parent = 0, const char *name = 0);
	virtual ~FirewalldClient();

	/* connection state */
	bool busConnected() const;
	bool isRunning();
	TQString version();
	TQString state();
	static bool polkitAgentRunning();

	void setMode(Mode mode);
	Mode mode() const { return m_mode; }

	TQString lastError() const { return m_lastError; }

	/** true after firewalld refused a request because authentication failed
	 *  or was cancelled; further requests are suppressed until
	 *  resetAuthorization() is called, so the user is not asked again and
	 *  again during a single refresh */
	bool authorizationDenied() const { return m_authDenied; }
	void resetAuthorization() { m_authDenied = false; }
	TQString lastErrorName() const { return m_lastErrorName; }

	/* read access (synchronous) */
	TQStringList zones();
	FwZone zone(const TQString &name);
	TQMap<TQString, TQStringList> activeZoneInterfaces();
	TQMap<TQString, TQStringList> activeZoneSources();
	TQString zoneOfInterface(const TQString &iface);
	TQStringList services();
	FwService service(const TQString &name);
	TQString defaultZone();
	bool panicMode();
	TQString logDenied();
	static TQStringList systemInterfaces();

	/* write access (asynchronous, mode dependent where applicable) */
	void addService(const TQString &zone, const TQString &service);
	void removeService(const TQString &zone, const TQString &service);
	void addPort(const TQString &zone, const TQString &port, const TQString &proto);
	void removePort(const TQString &zone, const TQString &port, const TQString &proto);
	void setMasquerade(const TQString &zone, bool enable);
	void addForwardPort(const TQString &zone, const FwForwardPort &fp);
	void removeForwardPort(const TQString &zone, const FwForwardPort &fp);
	void addRichRule(const TQString &zone, const TQString &rule);
	void removeRichRule(const TQString &zone, const TQString &rule);
	void addInterface(const TQString &zone, const TQString &iface);
	void removeInterface(const TQString &zone, const TQString &iface);
	void changeZoneOfInterface(const TQString &zone, const TQString &iface);
	void addSource(const TQString &zone, const TQString &source);
	void removeSource(const TQString &zone, const TQString &source);
	void setTarget(const TQString &zone, const TQString &target);

	/* global write access (asynchronous) */
	void setDefaultZone(const TQString &zone);
	void setPanicMode(bool enable);
	void setLogDenied(const TQString &value);
	void reload();
	void runtimeToPermanent();

	/* start firewalld.service through systemd (polkit protected) */
	bool startFirewalld();

	/* helpers, public for unit testing */
	static TQString friendlyError(const TQString &name, const TQString &message);

signals:
	/** emitted when an asynchronous request finished */
	void operationFinished(bool success, const TQString &description, const TQString &error);
	/** emitted (debounced) when firewalld announces a configuration change */
	void configurationChanged();
	/** emitted when a read request was refused by polkit */
	void authorizationRequired();
	/** emitted when firewalld appears on or disappears from the bus */
	void availabilityChanged(bool running);

private slots:
	void slotAsyncReply(const TQT_DBusMessage &reply);
	void slotDBusSignal(const TQT_DBusMessage &message);
	void slotEmitChanged();

private:
	TQT_DBusMessage call(const TQString &path, const TQString &interface,
	                     const TQString &method,
	                     const TQValueList<TQT_DBusData> &args = TQValueList<TQT_DBusData>());
	bool callAsync(const TQString &path, const TQString &interface,
	               const TQString &method, const TQValueList<TQT_DBusData> &args,
	               const TQString &description);
	TQString configZonePath(const TQString &zone);
	TQString configServicePath(const TQString &service);
	TQStringList stringListCall(const TQString &path, const TQString &interface,
	                            const TQString &method,
	                            const TQValueList<TQT_DBusData> &args = TQValueList<TQT_DBusData>());

	/* zone call helper: runtime uses the .zone interface with the zone name as
	 * first argument (and an optional timeout as last argument), permanent
	 * uses the per-zone config object */
	void zoneCall(const TQString &zone, const TQString &method,
	              const TQValueList<TQT_DBusData> &args, bool runtimeTimeout,
	              const TQString &description);

	TQMap<TQString, TQStringList> activeZoneEntries(const TQString &key);
	void addMatchRules();

	TQT_DBusConnection m_bus;
	Mode m_mode;
	TQString m_lastError;
	TQString m_lastErrorName;
	TQMap<int, TQString> m_pending;
	TQTimer *m_changeTimer;
	bool m_running;
	bool m_authDenied;
};

#endif
