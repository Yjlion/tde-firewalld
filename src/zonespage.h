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

#ifndef ZONESPAGE_H
#define ZONESPAGE_H

#include <tqwidget.h>

#include "firewalldclient.h"

class KComboBox;
class KPushButton;
class TDEListBox;
class TDEListView;
class TQCheckBox;
class TQLabel;
class TQListBoxItem;
class TQTabWidget;

/** A list view with "Add" and "Remove" buttons */
class ListEditor : public TQWidget
{
	TQ_OBJECT
public:
	ListEditor(TQWidget *parent, const TQString &intro);
	TDEListView *list() const { return m_list; }

signals:
	void addClicked();
	void removeClicked();

private slots:
	void slotSelectionChanged();

private:
	TDEListView *m_list;
	KPushButton *m_add;
	KPushButton *m_remove;
};

class ZonesPage : public TQWidget
{
	TQ_OBJECT
public:
	ZonesPage(FirewalldClient *client, TQWidget *parent = 0, const char *name = 0);

	/* called by the service check items */
	void serviceToggled(const TQString &service, bool on);
	bool updating() const { return m_updating; }
	/** zone to select on the next refresh */
	void selectZone(const TQString &zone) { m_zone.name = zone; }

public slots:
	void refresh();
	/** forget cached service definitions (e.g. after a mode change) */
	void clearCache();

private slots:
	void slotZoneSelected();
	void slotTarget(int index);
	void slotMasquerade(bool on);
	void slotAddPort();
	void slotRemovePort();
	void slotAddForward();
	void slotRemoveForward();
	void slotAddRichRule();
	void slotRemoveRichRule();
	void slotAddInterface();
	void slotRemoveInterface();
	void slotAddSource();
	void slotRemoveSource();

private:
	void loadZone(const TQString &name);
	TQString currentZone() const;

	FirewalldClient *m_client;
	TDEListBox *m_zones;
	TQLabel *m_title;
	TQLabel *m_description;
	KComboBox *m_target;
	TQTabWidget *m_tabs;
	TDEListView *m_services;
	ListEditor *m_ports;
	TQCheckBox *m_masquerade;
	ListEditor *m_forward;
	ListEditor *m_rich;
	ListEditor *m_interfaces;
	ListEditor *m_sources;
	TQMap<TQString, TQString> m_servicePorts;
	FwZone m_zone;
	TQString m_defaultZone;
	bool m_updating;
};

#endif
