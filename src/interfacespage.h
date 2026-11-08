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

#ifndef INTERFACESPAGE_H
#define INTERFACESPAGE_H

#include <tqwidget.h>

class FirewalldClient;
class KPushButton;
class TDEListView;
class TQListViewItem;

/** Shows which zone each network interface belongs to */
class InterfacesPage : public TQWidget
{
	TQ_OBJECT
public:
	InterfacesPage(FirewalldClient *client, TQWidget *parent = 0, const char *name = 0);

public slots:
	void refresh();

private slots:
	void slotChangeZone();
	void slotSelectionChanged();
	void slotExecuted(TQListViewItem *item);

private:
	FirewalldClient *m_client;
	TDEListView *m_list;
	KPushButton *m_change;
};

#endif
