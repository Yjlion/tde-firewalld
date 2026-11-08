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

#include "interfacespage.h"
#include "firewalldclient.h"
#include "dialogs.h"

#include <tqlabel.h>
#include <tqlayout.h>

#include <kdialog.h>
#include <kiconloader.h>
#include <kpushbutton.h>
#include <tdelistview.h>
#include <tdelocale.h>

InterfacesPage::InterfacesPage(FirewalldClient *client, TQWidget *parent, const char *name)
	: TQWidget(parent, name), m_client(client)
{
	TQGridLayout *grid = new TQGridLayout(this, 3, 2, KDialog::marginHint(), KDialog::spacingHint());

	TQLabel *intro = new TQLabel(i18n("Network interfaces of this computer and the zone that handles "
	                                  "their traffic. Interfaces that are not bound to a zone use the "
	                                  "default zone."), this);
	intro->setAlignment(TQt::WordBreak);
	grid->addMultiCellWidget(intro, 0, 0, 0, 1);

	m_list = new TDEListView(this);
	m_list->addColumn(i18n("Interface"));
	m_list->addColumn(i18n("Zone"));
	m_list->setAllColumnsShowFocus(true);
	m_list->setResizeMode(TQListView::LastColumn);
	grid->addMultiCellWidget(m_list, 1, 2, 0, 0);

	m_change = new KPushButton(SmallIconSet("edit"), i18n("&Change Zone..."), this);
	m_change->setEnabled(false);
	grid->addWidget(m_change, 1, 1);
	grid->setRowStretch(2, 1);

	connect(m_change, TQ_SIGNAL(clicked()), TQ_SLOT(slotChangeZone()));
	connect(m_list, TQ_SIGNAL(selectionChanged()), TQ_SLOT(slotSelectionChanged()));
	connect(m_list, TQ_SIGNAL(executed(TQListViewItem*)), TQ_SLOT(slotExecuted(TQListViewItem*)));
}

void InterfacesPage::refresh()
{
	TQString selected = m_list->selectedItem() ? m_list->selectedItem()->text(0) : TQString::null;
	m_list->clear();
	if (!m_client->isRunning()) {
		setEnabled(false);
		return;
	}
	setEnabled(true);

	TQString def = m_client->defaultZone();
	TQStringList ifaces = FirewalldClient::systemInterfaces();

	/* firewalld may know interfaces that do not exist (yet) */
	TQStringList zones = m_client->zones();
	for (TQStringList::const_iterator z = zones.begin(); z != zones.end(); ++z) {
		FwZone zone = m_client->zone(*z);
		for (TQStringList::const_iterator i = zone.interfaces.begin(); i != zone.interfaces.end(); ++i)
			if (!ifaces.contains(*i))
				ifaces << *i;
	}
	ifaces.sort();

	for (TQStringList::const_iterator it = ifaces.begin(); it != ifaces.end(); ++it) {
		TQString zone = m_client->zoneOfInterface(*it);
		TQString label = zone.isEmpty() ? i18n("%1 (default zone)").arg(def) : zone;
		TDEListViewItem *item = new TDEListViewItem(m_list, *it, label);
		item->setPixmap(0, SmallIcon("network"));
		if (*it == selected)
			m_list->setSelected(item, true);
	}
}

void InterfacesPage::slotSelectionChanged()
{
	m_change->setEnabled(m_list->selectedItem() != 0);
}

void InterfacesPage::slotExecuted(TQListViewItem *item)
{
	if (item)
		slotChangeZone();
}

void InterfacesPage::slotChangeZone()
{
	TQListViewItem *item = m_list->selectedItem();
	if (!item)
		return;
	TQString iface = item->text(0);
	TQString current = m_client->zoneOfInterface(iface);

	ZoneChooserDialog dlg(this, iface, m_client->zones(), current, true);
	if (dlg.exec() != TQDialog::Accepted)
		return;
	m_client->changeZoneOfInterface(dlg.zone(), iface);
}

#include "interfacespage.moc"
