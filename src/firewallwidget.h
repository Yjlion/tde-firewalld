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

#ifndef FIREWALLWIDGET_H
#define FIREWALLWIDGET_H

#include <tqwidget.h>
#include <tqmap.h>

class FirewalldClient;
class StatusPage;
class ZonesPage;
class ServicesPage;
class InterfacesPage;
class KComboBox;
class KPushButton;
class TQFrame;
class TQLabel;
class TQTabWidget;

/**
 * The complete firewall configuration user interface. It is embedded both
 * in the TDE Control Center module and in the standalone application.
 */
class FirewallWidget : public TQWidget
{
	TQ_OBJECT
public:
	FirewallWidget(TQWidget *parent = 0, const char *name = 0);
	virtual ~FirewallWidget();

	FirewalldClient *client() const { return m_client; }

	/** select a tab by name: status, zones, services, interfaces */
	void showPage(const TQString &page);
	/** show or hide the built-in message line (the main window uses its status bar) */
	void setMessageLineVisible(bool visible);
	/** select a zone in the zones tab */
	void selectZone(const TQString &zone);
	/** switch between the runtime and the permanent configuration */
	void setPermanent(bool permanent);

public slots:
	void refresh();
	void reloadFirewall();
	void runtimeToPermanent();

signals:
	void statusMessage(const TQString &message);

private slots:
	void slotModeChanged(int index);
	void slotTabChanged(TQWidget *page);
	void slotOperationFinished(bool success, const TQString &description, const TQString &error);
	void slotConfigurationChanged();
	void slotAvailabilityChanged(bool running);
	void slotStartFirewalld();
	void slotAuthenticate();

private:
	void updateBanners();
	void refreshPage(TQWidget *page);

	FirewalldClient *m_client;
	TQFrame *m_notRunning;
	TQLabel *m_notRunningText;
	TQFrame *m_noAgent;
	TQFrame *m_needAuth;
	KComboBox *m_mode;
	KPushButton *m_reload;
	KPushButton *m_toPermanent;
	TQTabWidget *m_tabs;
	StatusPage *m_status;
	ZonesPage *m_zones;
	ServicesPage *m_services;
	InterfacesPage *m_interfaces;
	TQLabel *m_message;
	TQMap<TQWidget*, bool> m_dirty;
};

#endif
