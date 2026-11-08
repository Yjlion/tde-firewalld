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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <tdemainwindow.h>

class FirewallWidget;

class MainWindow : public TDEMainWindow
{
	TQ_OBJECT
public:
	MainWindow();
	FirewallWidget *firewallWidget() const { return m_widget; }

private slots:
	void slotStatusMessage(const TQString &message);

private:
	FirewallWidget *m_widget;
};

#endif
