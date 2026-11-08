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

#ifndef FIREWALLTRAY_H
#define FIREWALLTRAY_H

#include <ksystemtray.h>

#include <tqstringlist.h>

class FirewalldClient;
class KHelpMenu;
class TDEPopupMenu;
class TDEToggleAction;
class TDEAction;

/**
 * System tray icon for kicker's system tray. Shows the firewall state and
 * offers quick access to the default zone, panic mode and the manager.
 *
 * It only uses requests covered by firewalld's "info" polkit action, so it
 * never asks for a password just by running.
 */
class FirewallTray : public KSystemTray
{
	TQ_OBJECT
public:
	FirewallTray(TQWidget *parent = 0, const char *name = 0);
	virtual ~FirewallTray();

	/** autostart setting (tde-firewalld-trayrc, [General] Autostart, default false) */
	static bool autostartEnabled();
	static void setAutostartEnabled(bool enable);

public slots:
	void updateState();
	void openManager();

protected:
	virtual void mousePressEvent(TQMouseEvent *e);
	virtual void contextMenuAboutToShow(TDEPopupMenu *menu);

private slots:
	void slotZoneSelected(int id);
	void slotPanicMode();
	void slotReload();
	void slotHelp();
	void slotAutostart();
	void slotOperationFinished(bool success, const TQString &description, const TQString &error);

private:
	TQPixmap statePixmap() const;

	FirewalldClient *m_client;
	TDEPopupMenu *m_zoneMenu;
	KHelpMenu *m_helpMenu;
	TDEAction *m_open;
	TDEToggleAction *m_panic;
	TDEAction *m_reload;
	TDEToggleAction *m_autostart;
	TQStringList m_zones;
	TQString m_defaultZone;
	bool m_running;
	bool m_panicOn;
};

#endif
