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

#include "statuspage.h"
#include "firewalldclient.h"

#include <tqcheckbox.h>
#include <tqgroupbox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqwhatsthis.h>

#include <kcombobox.h>
#include <kled.h>
#include <tdelistview.h>
#include <tdelocale.h>
#include <tdemessagebox.h>

static const char *logDeniedValues[] = { "off", "all", "unicast", "broadcast", "multicast", 0 };

StatusPage::StatusPage(FirewalldClient *client, TQWidget *parent, const char *name)
	: TQWidget(parent, name), m_client(client), m_updating(false)
{
	TQVBoxLayout *top = new TQVBoxLayout(this, 6, 6);

	/* --- daemon --- */
	TQGroupBox *daemon = new TQGroupBox(2, TQt::Horizontal, i18n("firewalld Service"), this);
	top->addWidget(daemon);

	new TQLabel(i18n("Status:"), daemon);
	TQWidget *stateBox = new TQWidget(daemon);
	TQHBoxLayout *sl = new TQHBoxLayout(stateBox, 0, 6);
	m_led = new KLed(stateBox);
	m_led->setShape(KLed::Circular);
	m_led->setLook(KLed::Sunken);
	m_state = new TQLabel(stateBox);
	sl->addWidget(m_led);
	sl->addWidget(m_state, 1);

	new TQLabel(i18n("Version:"), daemon);
	m_version = new TQLabel(daemon);

	/* --- global settings --- */
	TQGroupBox *settings = new TQGroupBox(2, TQt::Horizontal, i18n("Global Settings"), this);
	top->addWidget(settings);

	TQLabel *l = new TQLabel(i18n("&Default zone:"), settings);
	m_defaultZone = new KComboBox(false, settings);
	l->setBuddy(m_defaultZone);
	TQWhatsThis::add(m_defaultZone, i18n("The default zone is used for every connection and interface "
	                                      "that is not explicitly bound to another zone."));

	l = new TQLabel(i18n("&Log denied packets:"), settings);
	m_logDenied = new KComboBox(false, settings);
	m_logDenied->insertItem(i18n("Off"));
	m_logDenied->insertItem(i18n("All"));
	m_logDenied->insertItem(i18n("Unicast"));
	m_logDenied->insertItem(i18n("Broadcast"));
	m_logDenied->insertItem(i18n("Multicast"));
	l->setBuddy(m_logDenied);
	TQWhatsThis::add(m_logDenied, i18n("Log packets that are rejected or dropped by the firewall to the kernel log."));

	new TQLabel(i18n("Panic mode:"), settings);
	m_panic = new TQCheckBox(i18n("&Block all network traffic"), settings);
	TQWhatsThis::add(m_panic, i18n("Panic mode drops all incoming and outgoing packets. "
	                                "Active connections will be terminated."));

	/* --- active zones --- */
	TQGroupBox *active = new TQGroupBox(1, TQt::Horizontal, i18n("Active Zones"), this);
	top->addWidget(active, 1);
	m_active = new TDEListView(active);
	m_active->addColumn(i18n("Zone"));
	m_active->addColumn(i18n("Interfaces"));
	m_active->addColumn(i18n("Sources"));
	m_active->setAllColumnsShowFocus(true);
	m_active->setResizeMode(TQListView::LastColumn);

	connect(m_defaultZone, TQ_SIGNAL(activated(int)), TQ_SLOT(slotDefaultZone(int)));
	connect(m_logDenied, TQ_SIGNAL(activated(int)), TQ_SLOT(slotLogDenied(int)));
	connect(m_panic, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotPanicMode(bool)));
}

void StatusPage::refresh()
{
	m_updating = true;

	bool running = m_client->isRunning();
	TQString state = running ? m_client->state() : TQString::null;
	m_led->setColor(running ? ((state == "RUNNING" || state.isEmpty()) ? TQt::green : TQt::yellow) : TQt::red);
	m_led->on();
	if (!running)
		m_state->setText(i18n("Not running"));
	else if (state == "RUNNING" || state.isEmpty())
		m_state->setText(i18n("Running"));
	else
		m_state->setText(i18n("Running (%1)").arg(state));
	TQString version = running ? m_client->version() : TQString::null;
	m_version->setText(version.isEmpty() ? TQString("-") : version);

	m_defaultZone->clear();
	m_active->clear();
	bool logKnown = false;
	setEnabled(true);

	if (running) {
		/* default zone and zone list are global (runtime) settings */
		FirewalldClient::Mode mode = m_client->mode();
		m_client->setMode(FirewalldClient::Runtime);
		TQStringList zones = m_client->zones();
		m_client->setMode(mode);

		TQString def = m_client->defaultZone();
		m_defaultZone->insertStringList(zones);
		int idx = zones.findIndex(def);
		if (idx >= 0)
			m_defaultZone->setCurrentItem(idx);

		TQString log = m_client->logDenied();
		for (int i = 0; logDeniedValues[i]; ++i)
			if (log == logDeniedValues[i])
				m_logDenied->setCurrentItem(i);
		/* unknown when reading it was not authorized */
		logKnown = !log.isEmpty();

		m_panic->setChecked(m_client->panicMode());

		TQMap<TQString, TQStringList> ifaces = m_client->activeZoneInterfaces();
		TQMap<TQString, TQStringList> sources = m_client->activeZoneSources();
		for (TQMap<TQString, TQStringList>::const_iterator it = ifaces.begin(); it != ifaces.end(); ++it) {
			TQString zone = it.key();
			if (zone == def)
				zone = i18n("%1 (default)").arg(zone);
			new TDEListViewItem(m_active, zone, it.data().join(", "), sources[it.key()].join(", "));
		}
	}

	m_defaultZone->setEnabled(running);
	m_logDenied->setEnabled(running && logKnown);
	m_panic->setEnabled(running);
	m_updating = false;
}

void StatusPage::slotDefaultZone(int)
{
	if (m_updating)
		return;
	m_client->setDefaultZone(m_defaultZone->currentText());
}

void StatusPage::slotLogDenied(int index)
{
	if (m_updating || index < 0 || index > 4)
		return;
	m_client->setLogDenied(logDeniedValues[index]);
}

void StatusPage::slotPanicMode(bool on)
{
	if (m_updating)
		return;
	if (on) {
		int res = KMessageBox::warningContinueCancel(this,
			i18n("Panic mode blocks <b>all</b> incoming and outgoing network traffic "
			     "and terminates active connections.<br><br>Do you really want to enable it?"),
			i18n("Enable Panic Mode"), KGuiItem(i18n("Enable Panic Mode"), "process-stop"));
		if (res != KMessageBox::Continue) {
			m_updating = true;
			m_panic->setChecked(false);
			m_updating = false;
			return;
		}
	}
	m_client->setPanicMode(on);
}

#include "statuspage.moc"
