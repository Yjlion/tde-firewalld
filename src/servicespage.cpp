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

#include "servicespage.h"
#include "firewalldclient.h"

#include <tqlabel.h>
#include <tqlayout.h>
#include <tqsplitter.h>
#include <tqstylesheet.h>
#include <tqtextbrowser.h>

#include <kdialog.h>
#include <klineedit.h>
#include <tdelistbox.h>
#include <tdelocale.h>

ServicesPage::ServicesPage(FirewalldClient *client, TQWidget *parent, const char *name)
	: TQWidget(parent, name), m_client(client)
{
	TQHBoxLayout *top = new TQHBoxLayout(this, 0, 0);
	TQSplitter *split = new TQSplitter(TQt::Horizontal, this);
	top->addWidget(split);

	TQWidget *left = new TQWidget(split);
	TQVBoxLayout *ll = new TQVBoxLayout(left, KDialog::marginHint(), KDialog::spacingHint());
	TQLabel *fl = new TQLabel(i18n("&Search:"), left);
	m_filter = new KLineEdit(left);
	fl->setBuddy(m_filter);
	TQHBoxLayout *fh = new TQHBoxLayout(ll);
	fh->addWidget(fl);
	fh->addWidget(m_filter, 1);
	m_list = new TDEListBox(left);
	ll->addWidget(m_list, 1);

	TQWidget *right = new TQWidget(split);
	TQVBoxLayout *rl = new TQVBoxLayout(right, KDialog::marginHint(), KDialog::spacingHint());
	m_details = new TQTextBrowser(right);
	rl->addWidget(m_details, 1);

	split->setResizeMode(left, TQSplitter::KeepSize);
	TQValueList<int> sizes;
	sizes << 200 << 500;
	split->setSizes(sizes);

	connect(m_list, TQ_SIGNAL(selectionChanged()), TQ_SLOT(slotServiceSelected()));
	connect(m_filter, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotFilter(const TQString&)));
}

void ServicesPage::refresh()
{
	if (!m_client->isRunning()) {
		m_list->clear();
		m_details->clear();
		setEnabled(false);
		return;
	}
	setEnabled(true);
	m_services = m_client->services();
	slotFilter(m_filter->text());
}

void ServicesPage::slotFilter(const TQString &text)
{
	TQString keep = m_selected;
	m_list->blockSignals(true);
	m_list->clear();
	int sel = -1;
	for (TQStringList::const_iterator it = m_services.begin(); it != m_services.end(); ++it) {
		if (!text.isEmpty() && (*it).find(text, 0, false) < 0)
			continue;
		m_list->insertItem(*it);
		if (*it == keep)
			sel = m_list->count() - 1;
	}
	m_list->blockSignals(false);
	if (sel < 0 && m_list->count() > 0)
		sel = 0;
	if (sel >= 0) {
		m_list->setSelected(sel, true);
		m_list->ensureCurrentVisible();
	} else {
		m_details->clear();
	}
}

static TQString row(const TQString &label, const TQString &value)
{
	return TQString("<tr><td valign=\"top\"><b>%1</b></td><td>%2</td></tr>")
	       .arg(label).arg(value.isEmpty() ? TQString("-") : value);
}

void ServicesPage::slotServiceSelected()
{
	TQListBoxItem *item = m_list->selectedItem();
	if (!item)
		return;
	m_selected = item->text();
	FwService svc = m_client->service(m_selected);

	TQStringList ports;
	for (FwPortList::const_iterator it = svc.ports.begin(); it != svc.ports.end(); ++it)
		ports << ((*it).port.isEmpty() ? (*it).protocol : (*it).port + "/" + (*it).protocol);
	TQStringList sports;
	for (FwPortList::const_iterator it = svc.sourcePorts.begin(); it != svc.sourcePorts.end(); ++it)
		sports << (*it).port + "/" + (*it).protocol;
	TQStringList dests;
	for (TQMap<TQString, TQString>::const_iterator it = svc.destinations.begin(); it != svc.destinations.end(); ++it)
		dests << it.key() + ": " + it.data();

	TQString html = TQString("<h2>%1</h2>").arg(TQStyleSheet::escape(svc.shortName.isEmpty() ? svc.name : svc.shortName));
	html += TQString("<p>%1</p>").arg(TQStyleSheet::escape(svc.description));
	html += "<table cellspacing=\"4\">";
	html += row(i18n("Name:"), TQStyleSheet::escape(svc.name));
	html += row(i18n("Ports:"), ports.join(", "));
	html += row(i18n("Protocols:"), svc.protocols.join(", "));
	html += row(i18n("Source ports:"), sports.join(", "));
	html += row(i18n("Helper modules:"), svc.modules.join(", "));
	html += row(i18n("Destinations:"), dests.join(", "));
	html += row(i18n("Includes:"), svc.includes.join(", "));
	html += "</table>";
	m_details->setText(html);
}

#include "servicespage.moc"
