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

#include "zonespage.h"
#include "dialogs.h"

#include <tqcheckbox.h>
#include <tqfont.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqsplitter.h>
#include <tqtabwidget.h>
#include <tqwhatsthis.h>

#include <kcombobox.h>
#include <kdialog.h>
#include <kiconloader.h>
#include <kpushbutton.h>
#include <kstdguiitem.h>
#include <tdelistbox.h>
#include <tdelistview.h>
#include <tdelocale.h>
#include <tdemessagebox.h>

static const char *targets[] = { "default", "ACCEPT", "DROP", "%%REJECT%%", 0 };

/* ---------------------------------------------------------------------- */

class ServiceCheckItem : public TQCheckListItem
{
public:
	ServiceCheckItem(TQListView *parent, ZonesPage *page, const TQString &name, const TQString &ports)
		: TQCheckListItem(parent, name, TQCheckListItem::CheckBox), m_page(page)
	{
		setText(1, ports);
	}

	/* enabled services first, then alphabetically (case-insensitive) */
	virtual int compare(TQListViewItem *other, int col, bool ascending) const
	{
		if (col == 0 && other->rtti() == rtti()) {
			bool otherOn = static_cast<ServiceCheckItem*>(other)->isOn();
			if (isOn() != otherOn)
				return (isOn() ? -1 : 1) * (ascending ? 1 : -1);
		}
		return text(col).lower().localeAwareCompare(other->text(col).lower());
	}

	virtual int rtti() const { return 1001; }

protected:
	virtual void stateChange(bool on)
	{
		TQCheckListItem::stateChange(on);
		if (!m_page->updating())
			m_page->serviceToggled(text(0), on);
	}

private:
	ZonesPage *m_page;
};

/* ---------------------------------------------------------------------- */

ListEditor::ListEditor(TQWidget *parent, const TQString &intro)
	: TQWidget(parent)
{
	TQGridLayout *grid = new TQGridLayout(this, 4, 2, KDialog::marginHint(), KDialog::spacingHint());

	int row = 0;
	if (!intro.isEmpty()) {
		TQLabel *l = new TQLabel(intro, this);
		l->setAlignment(TQt::WordBreak);
		grid->addMultiCellWidget(l, 0, 0, 0, 1);
		row = 1;
	}

	m_list = new TDEListView(this);
	m_list->setAllColumnsShowFocus(true);
	m_list->setResizeMode(TQListView::LastColumn);
	grid->addMultiCellWidget(m_list, row, row + 2, 0, 0);

	m_add = new KPushButton(KGuiItem(i18n("&Add..."), "add"), this);
	m_remove = new KPushButton(KStdGuiItem::remove(), this);
	m_remove->setEnabled(false);
	grid->addWidget(m_add, row, 1);
	grid->addWidget(m_remove, row + 1, 1);
	grid->setRowStretch(row + 2, 1);

	connect(m_add, TQ_SIGNAL(clicked()), TQ_SIGNAL(addClicked()));
	connect(m_remove, TQ_SIGNAL(clicked()), TQ_SIGNAL(removeClicked()));
	connect(m_list, TQ_SIGNAL(selectionChanged()), TQ_SLOT(slotSelectionChanged()));
}

void ListEditor::slotSelectionChanged()
{
	m_remove->setEnabled(m_list->selectedItem() != 0);
}

/* ---------------------------------------------------------------------- */

ZonesPage::ZonesPage(FirewalldClient *client, TQWidget *parent, const char *name)
	: TQWidget(parent, name), m_client(client), m_updating(false)
{
	TQHBoxLayout *top = new TQHBoxLayout(this, 0, 0);
	TQSplitter *split = new TQSplitter(TQt::Horizontal, this);
	top->addWidget(split);

	/* zone list */
	TQWidget *left = new TQWidget(split);
	TQVBoxLayout *ll = new TQVBoxLayout(left, KDialog::marginHint(), KDialog::spacingHint());
	TQLabel *zl = new TQLabel(i18n("Zones:"), left);
	m_zones = new TDEListBox(left);
	zl->setBuddy(m_zones);
	ll->addWidget(zl);
	ll->addWidget(m_zones, 1);
	TQWhatsThis::add(m_zones, i18n("A zone defines the trust level of the network connections, "
	                                "interfaces and source addresses bound to it."));

	/* zone details */
	TQWidget *right = new TQWidget(split);
	TQVBoxLayout *rl = new TQVBoxLayout(right, KDialog::marginHint(), KDialog::spacingHint());

	m_title = new TQLabel(right);
	TQFont f = m_title->font();
	f.setBold(true);
	f.setPointSize(f.pointSize() + 2);
	m_title->setFont(f);
	rl->addWidget(m_title);

	m_description = new TQLabel(right);
	m_description->setAlignment(TQt::WordBreak | TQt::AlignTop);
	rl->addWidget(m_description);

	TQHBoxLayout *tl = new TQHBoxLayout(rl);
	TQLabel *targetLabel = new TQLabel(i18n("Tar&get:"), right);
	m_target = new KComboBox(false, right);
	m_target->insertItem(i18n("Default (reject, allow ICMP)"));
	m_target->insertItem(i18n("ACCEPT - accept all packets"));
	m_target->insertItem(i18n("DROP - silently drop packets"));
	m_target->insertItem(i18n("REJECT - reject packets"));
	targetLabel->setBuddy(m_target);
	TQWhatsThis::add(m_target, i18n("What happens to packets that do not match any rule of the zone. "
	                                 "The target can only be changed in the permanent configuration."));
	tl->addWidget(targetLabel);
	tl->addWidget(m_target);
	tl->addStretch(1);

	m_tabs = new TQTabWidget(right);
	rl->addWidget(m_tabs, 1);

	/* services */
	TQWidget *svcPage = new TQWidget(m_tabs);
	TQVBoxLayout *svl = new TQVBoxLayout(svcPage, KDialog::marginHint(), KDialog::spacingHint());
	TQLabel *svcIntro = new TQLabel(i18n("Check the services that should be reachable from machines "
	                                     "in this zone."), svcPage);
	svcIntro->setAlignment(TQt::WordBreak);
	svl->addWidget(svcIntro);
	m_services = new TDEListView(svcPage);
	m_services->addColumn(i18n("Service"));
	m_services->addColumn(i18n("Ports"));
	m_services->setAllColumnsShowFocus(true);
	m_services->setResizeMode(TQListView::LastColumn);
	svl->addWidget(m_services, 1);
	m_tabs->addTab(svcPage, SmallIconSet("network"), i18n("&Services"));

	/* ports */
	m_ports = new ListEditor(m_tabs, i18n("Additional ports or port ranges that are open in this zone."));
	m_ports->list()->addColumn(i18n("Port"), 140);
	m_ports->list()->addColumn(i18n("Protocol"));
	m_tabs->addTab(m_ports, i18n("&Ports"));

	/* masquerading */
	TQWidget *masqPage = new TQWidget(m_tabs);
	TQVBoxLayout *ml = new TQVBoxLayout(masqPage, KDialog::marginHint(), KDialog::spacingHint());
	TQLabel *masqIntro = new TQLabel(i18n("Masquerading (IPv4 NAT) lets machines behind this host reach "
	                                      "the network with the address of this host. It is needed to "
	                                      "share an Internet connection and for port forwarding to "
	                                      "other hosts."), masqPage);
	masqIntro->setAlignment(TQt::WordBreak);
	m_masquerade = new TQCheckBox(i18n("Enable mas&querading"), masqPage);
	ml->addWidget(masqIntro);
	ml->addWidget(m_masquerade);
	ml->addStretch(1);
	m_tabs->addTab(masqPage, i18n("&Masquerading"));

	/* port forwarding */
	m_forward = new ListEditor(m_tabs, i18n("Forward incoming connections to another port or host."));
	m_forward->list()->addColumn(i18n("Port"));
	m_forward->list()->addColumn(i18n("Protocol"));
	m_forward->list()->addColumn(i18n("To Port"));
	m_forward->list()->addColumn(i18n("To Address"));
	m_forward->list()->setResizeMode(TQListView::AllColumns);
	m_tabs->addTab(m_forward, i18n("Port Forwar&ding"));

	/* rich rules */
	m_rich = new ListEditor(m_tabs, i18n("Rich rules allow complex rules, e.g. allowing a service only "
	                                     "for certain source addresses, logging and rate limiting."));
	m_rich->list()->addColumn(i18n("Rule"));
	m_tabs->addTab(m_rich, i18n("&Rich Rules"));

	/* interfaces */
	m_interfaces = new ListEditor(m_tabs, i18n("Network interfaces bound to this zone. Interfaces managed by "
	                                           "NetworkManager are usually bound through the connection "
	                                           "settings instead."));
	m_interfaces->list()->addColumn(i18n("Interface"));
	m_tabs->addTab(m_interfaces, i18n("Interfa&ces"));

	/* sources */
	m_sources = new ListEditor(m_tabs, i18n("Traffic coming from these source addresses (IP address, "
	                                        "network in CIDR notation, MAC address or ipset:name) is "
	                                        "handled by this zone."));
	m_sources->list()->addColumn(i18n("Source"));
	m_tabs->addTab(m_sources, i18n("S&ources"));

	split->setResizeMode(left, TQSplitter::KeepSize);
	TQValueList<int> sizes;
	sizes << 170 << 530;
	split->setSizes(sizes);

	connect(m_zones, TQ_SIGNAL(selectionChanged()), TQ_SLOT(slotZoneSelected()));
	connect(m_target, TQ_SIGNAL(activated(int)), TQ_SLOT(slotTarget(int)));
	connect(m_masquerade, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotMasquerade(bool)));
	connect(m_ports, TQ_SIGNAL(addClicked()), TQ_SLOT(slotAddPort()));
	connect(m_ports, TQ_SIGNAL(removeClicked()), TQ_SLOT(slotRemovePort()));
	connect(m_forward, TQ_SIGNAL(addClicked()), TQ_SLOT(slotAddForward()));
	connect(m_forward, TQ_SIGNAL(removeClicked()), TQ_SLOT(slotRemoveForward()));
	connect(m_rich, TQ_SIGNAL(addClicked()), TQ_SLOT(slotAddRichRule()));
	connect(m_rich, TQ_SIGNAL(removeClicked()), TQ_SLOT(slotRemoveRichRule()));
	connect(m_interfaces, TQ_SIGNAL(addClicked()), TQ_SLOT(slotAddInterface()));
	connect(m_interfaces, TQ_SIGNAL(removeClicked()), TQ_SLOT(slotRemoveInterface()));
	connect(m_sources, TQ_SIGNAL(addClicked()), TQ_SLOT(slotAddSource()));
	connect(m_sources, TQ_SIGNAL(removeClicked()), TQ_SLOT(slotRemoveSource()));
}

TQString ZonesPage::currentZone() const
{
	return m_zone.name;
}

void ZonesPage::refresh()
{
	m_updating = true;
	TQString selected = m_zone.name;

	m_zones->clear();
	if (!m_client->isRunning()) {
		m_zone = FwZone();
		m_title->setText(TQString::null);
		m_description->setText(TQString::null);
		m_services->clear();
		setEnabled(false);
		m_updating = false;
		return;
	}
	setEnabled(true);

	m_defaultZone = m_client->defaultZone();
	TQStringList zones = m_client->zones();
	if (selected.isEmpty() || !zones.contains(selected))
		selected = m_defaultZone;

	int sel = 0;
	for (uint i = 0; i < zones.count(); ++i) {
		TQString label = zones[i];
		if (zones[i] == m_defaultZone)
			m_zones->insertItem(SmallIcon("bookmark"), label);
		else
			m_zones->insertItem(label);
		if (zones[i] == selected)
			sel = i;
	}
	m_updating = false;
	if (m_zones->count() > 0) {
		m_zones->setSelected(sel, true);
		m_zones->ensureCurrentVisible();
	}
	loadZone(selected);
}

void ZonesPage::clearCache()
{
	m_servicePorts.clear();
}

void ZonesPage::slotZoneSelected()
{
	if (m_updating)
		return;
	TQListBoxItem *item = m_zones->selectedItem();
	if (item)
		loadZone(item->text());
}

void ZonesPage::loadZone(const TQString &name)
{
	m_updating = true;
	m_zone = m_client->zone(name);

	TQString title = m_zone.shortName.isEmpty() ? name : m_zone.shortName;
	if (name == m_defaultZone)
		title = i18n("%1 (default zone)").arg(title);
	m_title->setText(title);
	m_description->setText(m_zone.description);

	int t = 0;
	for (int i = 0; targets[i]; ++i)
		if (m_zone.target == targets[i] || (i == 3 && m_zone.target == "REJECT"))
			t = i;
	m_target->setCurrentItem(t);
	m_target->setEnabled(m_zone.valid && m_client->mode() == FirewalldClient::Permanent);
	m_tabs->setEnabled(m_zone.valid);

	/* services */
	int contentsY = m_services->contentsY();
	m_services->clear();
	TQStringList services = m_client->services();
	/* also list enabled services that are not defined (anymore) */
	for (TQStringList::const_iterator it = m_zone.services.begin(); it != m_zone.services.end(); ++it)
		if (!services.contains(*it))
			services << *it;
	TQMap<TQString, TQString> sorted;
	for (TQStringList::const_iterator it = services.begin(); it != services.end(); ++it)
		sorted[(*it).lower() + TQChar(0) + *it] = *it;
	services = sorted.values();
	for (TQStringList::const_iterator it = services.begin(); it != services.end(); ++it) {
		if (!m_servicePorts.contains(*it)) {
			FwService svc = m_client->service(*it);
			TQStringList ports;
			for (FwPortList::const_iterator p = svc.ports.begin(); p != svc.ports.end(); ++p)
				ports << ((*p).port.isEmpty() ? (*p).protocol : (*p).port + "/" + (*p).protocol);
			ports += svc.protocols;
			if (svc.valid)
				m_servicePorts[*it] = ports.join(", ");
		}
		ServiceCheckItem *item = new ServiceCheckItem(m_services, this, *it,
		                                              m_servicePorts.contains(*it) ? m_servicePorts[*it] : TQString::null);
		item->setOn(m_zone.services.contains(*it));
	}
	m_services->sort();
	m_services->setContentsPos(0, contentsY);

	/* ports */
	m_ports->list()->clear();
	for (FwPortList::const_iterator it = m_zone.ports.begin(); it != m_zone.ports.end(); ++it)
		new TDEListViewItem(m_ports->list(), (*it).port, (*it).protocol);

	m_masquerade->setChecked(m_zone.masquerade);

	m_forward->list()->clear();
	for (FwForwardPortList::const_iterator it = m_zone.forwardPorts.begin(); it != m_zone.forwardPorts.end(); ++it)
		new TDEListViewItem(m_forward->list(), (*it).port, (*it).protocol, (*it).toPort, (*it).toAddr);

	m_rich->list()->clear();
	for (TQStringList::const_iterator it = m_zone.richRules.begin(); it != m_zone.richRules.end(); ++it)
		new TDEListViewItem(m_rich->list(), *it);

	m_interfaces->list()->clear();
	for (TQStringList::const_iterator it = m_zone.interfaces.begin(); it != m_zone.interfaces.end(); ++it)
		new TDEListViewItem(m_interfaces->list(), *it);

	m_sources->list()->clear();
	for (TQStringList::const_iterator it = m_zone.sources.begin(); it != m_zone.sources.end(); ++it)
		new TDEListViewItem(m_sources->list(), *it);

	m_updating = false;
}

void ZonesPage::serviceToggled(const TQString &service, bool on)
{
	if (m_zone.name.isEmpty())
		return;
	if (on)
		m_client->addService(m_zone.name, service);
	else
		m_client->removeService(m_zone.name, service);
}

void ZonesPage::slotTarget(int index)
{
	if (m_updating || index < 0 || index > 3 || m_zone.name.isEmpty())
		return;
	m_client->setTarget(m_zone.name, targets[index]);
}

void ZonesPage::slotMasquerade(bool on)
{
	if (m_updating || m_zone.name.isEmpty())
		return;
	m_client->setMasquerade(m_zone.name, on);
}

void ZonesPage::slotAddPort()
{
	PortDialog dlg(this, i18n("Open Port in Zone %1").arg(m_zone.name));
	if (dlg.exec() != TQDialog::Accepted)
		return;
	FwPort p = dlg.port();
	m_client->addPort(m_zone.name, p.port, p.protocol);
}

void ZonesPage::slotRemovePort()
{
	TQListViewItem *item = m_ports->list()->selectedItem();
	if (item)
		m_client->removePort(m_zone.name, item->text(0), item->text(1));
}

void ZonesPage::slotAddForward()
{
	ForwardPortDialog dlg(this);
	if (dlg.exec() != TQDialog::Accepted)
		return;
	FwForwardPort fp = dlg.forwardPort();
	if (!fp.toAddr.isEmpty() && !m_zone.masquerade) {
		KMessageBox::information(this,
			i18n("Forwarding to another host only works if masquerading is enabled for the zone."),
			TQString::null, "ForwardNeedsMasquerade");
	}
	m_client->addForwardPort(m_zone.name, fp);
}

void ZonesPage::slotRemoveForward()
{
	TQListViewItem *item = m_forward->list()->selectedItem();
	if (!item)
		return;
	FwForwardPort fp;
	fp.port = item->text(0);
	fp.protocol = item->text(1);
	fp.toPort = item->text(2);
	fp.toAddr = item->text(3);
	m_client->removeForwardPort(m_zone.name, fp);
}

void ZonesPage::slotAddRichRule()
{
	RichRuleDialog dlg(this);
	if (dlg.exec() == TQDialog::Accepted)
		m_client->addRichRule(m_zone.name, dlg.rule());
}

void ZonesPage::slotRemoveRichRule()
{
	TQListViewItem *item = m_rich->list()->selectedItem();
	if (item)
		m_client->removeRichRule(m_zone.name, item->text(0));
}

void ZonesPage::slotAddInterface()
{
	EntryDialog dlg(this, i18n("Bind Interface"), i18n("&Interface:"),
	                i18n("Name of the network interface, e.g. eth0 or wlan0."),
	                FirewalldClient::systemInterfaces());
	if (dlg.exec() == TQDialog::Accepted)
		m_client->addInterface(m_zone.name, dlg.text());
}

void ZonesPage::slotRemoveInterface()
{
	TQListViewItem *item = m_interfaces->list()->selectedItem();
	if (item)
		m_client->removeInterface(m_zone.name, item->text(0));
}

void ZonesPage::slotAddSource()
{
	EntryDialog dlg(this, i18n("Bind Source"), i18n("&Source address:"),
	                i18n("An IPv4 or IPv6 address, a network in CIDR notation "
	                     "(e.g. 192.168.1.0/24), a MAC address or ipset:name."));
	if (dlg.exec() == TQDialog::Accepted)
		m_client->addSource(m_zone.name, dlg.text());
}

void ZonesPage::slotRemoveSource()
{
	TQListViewItem *item = m_sources->list()->selectedItem();
	if (item)
		m_client->removeSource(m_zone.name, item->text(0));
}

#include "zonespage.moc"
