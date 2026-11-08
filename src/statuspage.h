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

#ifndef STATUSPAGE_H
#define STATUSPAGE_H

#include <tqwidget.h>

class FirewalldClient;
class KComboBox;
class KLed;
class TDEListView;
class TQCheckBox;
class TQLabel;

class StatusPage : public TQWidget
{
	TQ_OBJECT
public:
	StatusPage(FirewalldClient *client, TQWidget *parent = 0, const char *name = 0);

public slots:
	void refresh();

private slots:
	void slotDefaultZone(int index);
	void slotPanicMode(bool on);
	void slotLogDenied(int index);

private:
	FirewalldClient *m_client;
	KLed *m_led;
	TQLabel *m_state;
	TQLabel *m_version;
	KComboBox *m_defaultZone;
	TQCheckBox *m_panic;
	KComboBox *m_logDenied;
	TDEListView *m_active;
	bool m_updating;
};

#endif
