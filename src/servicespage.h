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

#ifndef SERVICESPAGE_H
#define SERVICESPAGE_H

#include <tqwidget.h>

class FirewalldClient;
class KLineEdit;
class TDEListBox;
class TQTextBrowser;

/** Read-only browser for the firewalld service definitions */
class ServicesPage : public TQWidget
{
	TQ_OBJECT
public:
	ServicesPage(FirewalldClient *client, TQWidget *parent = 0, const char *name = 0);

public slots:
	void refresh();

private slots:
	void slotServiceSelected();
	void slotFilter(const TQString &text);

private:
	FirewalldClient *m_client;
	KLineEdit *m_filter;
	TDEListBox *m_list;
	TQTextBrowser *m_details;
	TQStringList m_services;
	TQString m_selected;
};

#endif
