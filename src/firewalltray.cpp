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

#include "firewalltray.h"
#include "firewalldclient.h"
#include "dialogs.h"

#include <tqimage.h>
#include <tqpainter.h>
#include <tqstylesheet.h>
#include <tqtooltip.h>

#include <tdeaboutdata.h>
#include <tdeaction.h>
#include <tdeapplication.h>
#include <tdeconfig.h>
#include <tdeglobal.h>
#include <tdelocale.h>
#include <tdepopupmenu.h>
#include <khelpmenu.h>
#include <kiconeffect.h>
#include <kiconloader.h>
#include <kpassivepopup.h>
#include <krun.h>

static const char *TRAY_RC = "tde-firewalld-trayrc";

FirewallTray::FirewallTray(TQWidget *parent, const char *name)
	: KSystemTray(parent, name), m_running(false), m_panicOn(false)
{
	m_client = new FirewalldClient(this);

	m_open = new TDEAction(i18n("&Open Firewall Manager"), "tde-firewalld", 0,
	                       this, TQ_SLOT(openManager()), actionCollection(), "open_manager");
	m_panic = new TDEToggleAction(i18n("&Panic Mode"), "process-stop", 0,
	                              this, TQ_SLOT(slotPanicMode()), actionCollection(), "panic_mode");
	m_reload = new TDEAction(i18n("&Reload Firewall"), "reload", 0,
	                         this, TQ_SLOT(slotReload()), actionCollection(), "reload_firewall");
	m_autostart = new TDEToggleAction(i18n("&Start Automatically at Login"), 0,
	                                  this, TQ_SLOT(slotAutostart()), actionCollection(), "autostart");

	TDEPopupMenu *menu = contextMenu();
	m_open->plug(menu);
	menu->insertSeparator();
	m_zoneMenu = new TDEPopupMenu(menu);
	m_zoneMenu->setCheckable(true);
	menu->insertItem(SmallIconSet("network_local"), i18n("&Default Zone"), m_zoneMenu);
	m_panic->plug(menu);
	m_reload->plug(menu);
	menu->insertSeparator();
	m_autostart->plug(menu);
	/* the tray is documented in the Firewall Manager handbook */
	m_helpMenu = new KHelpMenu(this, TDEGlobal::instance()->aboutData(), false);
	TDEPopupMenu *help = new TDEPopupMenu(menu);
	help->insertItem(SmallIconSet("contents"), i18n("Firewall &Handbook"), this, TQ_SLOT(slotHelp()));
	help->insertSeparator();
	help->insertItem(SmallIconSet("tde-firewalld"), i18n("&About Firewall Tray"),
	                 m_helpMenu, TQ_SLOT(aboutApplication()));
	help->insertItem(SmallIconSet("about_tde"), i18n("About &TDE"), m_helpMenu, TQ_SLOT(aboutKDE()));
	menu->insertItem(SmallIconSet("help"), i18n("&Help"), help);

	connect(m_zoneMenu, TQ_SIGNAL(activated(int)), TQ_SLOT(slotZoneSelected(int)));
	connect(m_client, TQ_SIGNAL(configurationChanged()), TQ_SLOT(updateState()));
	connect(m_client, TQ_SIGNAL(availabilityChanged(bool)), TQ_SLOT(updateState()));
	connect(m_client, TQ_SIGNAL(operationFinished(bool, const TQString&, const TQString&)),
	        TQ_SLOT(slotOperationFinished(bool, const TQString&, const TQString&)));

	updateState();
}

FirewallTray::~FirewallTray()
{
}

bool FirewallTray::autostartEnabled()
{
	TDEConfig config(TRAY_RC, true, false);
	config.setGroup("General");
	return config.readBoolEntry("Autostart", false);
}

void FirewallTray::setAutostartEnabled(bool enable)
{
	TDEConfig config(TRAY_RC, false, false);
	config.setGroup("General");
	config.writeEntry("Autostart", enable);
	config.sync();
}

TQPixmap FirewallTray::statePixmap() const
{
	TQPixmap pix = KSystemTray::loadIcon("tde-firewalld");
	if (!m_running) {
		TQImage img = pix.convertToImage();
		TDEIconEffect::toGray(img, 1.0);
		TDEIconEffect::semiTransparent(img);
		pix.convertFromImage(img);
	} else if (m_panicOn) {
		int size = TQMAX(pix.width() / 2 + 2, 10);
		TQPixmap stop = TDEGlobal::iconLoader()->loadIcon("process-stop", TDEIcon::Small, size);
		TQPainter p(&pix);
		p.drawPixmap(pix.width() - stop.width(), pix.height() - stop.height(), stop);
	}
	return pix;
}

void FirewallTray::updateState()
{
	m_running = m_client->isRunning();
	m_zones.clear();
	m_defaultZone = TQString::null;
	m_panicOn = false;

	TQString tip;
	if (!m_running) {
		tip = i18n("<b>Firewall</b><br>firewalld is not running");
	} else {
		/* only "info" level requests: never triggers a password prompt */
		m_zones = m_client->zones();
		m_defaultZone = m_client->defaultZone();
		m_panicOn = m_client->panicMode();

		tip = i18n("<b>Firewall</b>");
		if (m_panicOn)
			tip += "<br>" + i18n("<font color=\"red\"><b>Panic mode: all traffic is blocked</b></font>");
		else
			tip += "<br>" + i18n("Active");
		tip += "<br>" + i18n("Default zone: %1").arg(TQStyleSheet::escape(m_defaultZone));

		TQMap<TQString, TQStringList> ifaces = m_client->activeZoneInterfaces();
		TQMap<TQString, TQStringList> sources = m_client->activeZoneSources();
		for (TQMap<TQString, TQStringList>::const_iterator it = ifaces.begin(); it != ifaces.end(); ++it) {
			TQStringList members = it.data() + sources[it.key()];
			tip += "<br>" + i18n("%1: %2").arg(TQStyleSheet::escape(it.key()))
			                              .arg(TQStyleSheet::escape(members.join(", ")));
		}
	}

	setPixmap(statePixmap());
	TQToolTip::remove(this);
	TQToolTip::add(this, tip);

	m_panic->setChecked(m_panicOn);
	m_panic->setEnabled(m_running);
	m_reload->setEnabled(m_running);
}

void FirewallTray::contextMenuAboutToShow(TDEPopupMenu *)
{
	m_zoneMenu->clear();
	for (uint i = 0; i < m_zones.count(); ++i) {
		m_zoneMenu->insertItem(m_zones[i], i);
		m_zoneMenu->setItemChecked(i, m_zones[i] == m_defaultZone);
	}
	if (m_zones.isEmpty()) {
		m_zoneMenu->insertItem(i18n("firewalld is not running"), 0);
		m_zoneMenu->setItemEnabled(0, false);
	}
	m_autostart->setChecked(autostartEnabled());
}

void FirewallTray::mousePressEvent(TQMouseEvent *e)
{
	if (e->button() == TQt::LeftButton)
		openManager();
	else
		KSystemTray::mousePressEvent(e);
}

void FirewallTray::openManager()
{
	KRun::runCommand("tde-firewalld", "tde-firewalld", "tde-firewalld");
}

void FirewallTray::slotZoneSelected(int id)
{
	if (id < 0 || id >= (int)m_zones.count() || m_zones[id] == m_defaultZone)
		return;
	m_client->setDefaultZone(m_zones[id]);
}

void FirewallTray::slotPanicMode()
{
	bool enable = m_panic->isChecked();
	if (enable && !confirmPanicMode(0)) {
		m_panic->setChecked(false);
		return;
	}
	m_client->setPanicMode(enable);
}

void FirewallTray::slotHelp()
{
	kapp->invokeHelp("tray", "tde-firewalld");
}

void FirewallTray::slotReload()
{
	m_client->reload();
}

void FirewallTray::slotAutostart()
{
	setAutostartEnabled(m_autostart->isChecked());
}

void FirewallTray::slotOperationFinished(bool success, const TQString &description, const TQString &error)
{
	if (!success)
		KPassivePopup::message(i18n("Firewall change failed"),
		                       TQString("%1\n%2").arg(description).arg(error),
		                       statePixmap(), this);
	updateState();
}

#include "firewalltray.moc"
