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

#include "dialogs.h"

#include <tqlabel.h>
#include <tqlayout.h>
#include <tqtextedit.h>
#include <tqwhatsthis.h>
#include <tqregexp.h>

#include <kcombobox.h>
#include <klineedit.h>
#include <tdelocale.h>
#include <tdemessagebox.h>

static void fillProtocols(KComboBox *box)
{
	box->insertItem("tcp");
	box->insertItem("udp");
	box->insertItem("sctp");
	box->insertItem("dccp");
}

/* ---------------------------------------------------------------------- */

PortDialog::PortDialog(TQWidget *parent, const TQString &caption)
	: KDialogBase(parent, "PortDialog", true, caption, Ok | Cancel, Ok, true)
{
	TQWidget *page = new TQWidget(this);
	setMainWidget(page);

	TQGridLayout *grid = new TQGridLayout(page, 3, 2, 0, spacingHint());

	TQLabel *l = new TQLabel(i18n("&Port or port range:"), page);
	m_port = new KLineEdit(page);
	l->setBuddy(m_port);
	TQWhatsThis::add(m_port, i18n("A single port number such as <b>8080</b> or a range such as <b>6000-6010</b>."));
	grid->addWidget(l, 0, 0);
	grid->addWidget(m_port, 0, 1);

	l = new TQLabel(i18n("P&rotocol:"), page);
	m_protocol = new KComboBox(false, page);
	fillProtocols(m_protocol);
	l->setBuddy(m_protocol);
	grid->addWidget(l, 1, 0);
	grid->addWidget(m_protocol, 1, 1);
	grid->setRowStretch(2, 1);

	connect(m_port, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotChanged()));
	m_port->setFocus();
	slotChanged();
	setMinimumWidth(380);
}

bool PortDialog::validPort(const TQString &port)
{
	TQRegExp re("^(\\d{1,5})(-(\\d{1,5}))?$");
	if (!re.exactMatch(port.stripWhiteSpace()))
		return false;
	int a = re.cap(1).toInt();
	if (a < 1 || a > 65535)
		return false;
	if (!re.cap(3).isEmpty()) {
		int b = re.cap(3).toInt();
		if (b < 1 || b > 65535 || b < a)
			return false;
	}
	return true;
}

void PortDialog::slotChanged()
{
	enableButtonOK(validPort(m_port->text()));
}

void PortDialog::slotOk()
{
	if (!validPort(m_port->text())) {
		KMessageBox::sorry(this, i18n("Please enter a port between 1 and 65535 or a range such as 6000-6010."));
		return;
	}
	KDialogBase::slotOk();
}

FwPort PortDialog::port() const
{
	FwPort p;
	p.port = m_port->text().stripWhiteSpace();
	p.protocol = m_protocol->currentText();
	return p;
}

/* ---------------------------------------------------------------------- */

ForwardPortDialog::ForwardPortDialog(TQWidget *parent)
	: KDialogBase(parent, "ForwardPortDialog", true, i18n("Add Port Forwarding"), Ok | Cancel, Ok, true)
{
	TQWidget *page = new TQWidget(this);
	setMainWidget(page);
	TQGridLayout *grid = new TQGridLayout(page, 6, 2, 0, spacingHint());

	TQLabel *intro = new TQLabel(i18n("Forward traffic arriving at a local port to another port "
	                                  "and/or another host. Forwarding to another host requires "
	                                  "masquerading to be enabled for the zone."), page);
	intro->setAlignment(TQt::WordBreak);
	grid->addMultiCellWidget(intro, 0, 0, 0, 1);

	TQLabel *l = new TQLabel(i18n("Source &port:"), page);
	m_port = new KLineEdit(page);
	l->setBuddy(m_port);
	grid->addWidget(l, 1, 0);
	grid->addWidget(m_port, 1, 1);

	l = new TQLabel(i18n("P&rotocol:"), page);
	m_protocol = new KComboBox(false, page);
	fillProtocols(m_protocol);
	l->setBuddy(m_protocol);
	grid->addWidget(l, 2, 0);
	grid->addWidget(m_protocol, 2, 1);

	l = new TQLabel(i18n("Destination p&ort:"), page);
	m_toPort = new KLineEdit(page);
	TQWhatsThis::add(m_toPort, i18n("Leave empty to keep the same port."));
	l->setBuddy(m_toPort);
	grid->addWidget(l, 3, 0);
	grid->addWidget(m_toPort, 3, 1);

	l = new TQLabel(i18n("Destination &address:"), page);
	m_toAddr = new KLineEdit(page);
	TQWhatsThis::add(m_toAddr, i18n("IPv4 address of the destination host. Leave empty to forward to a local port."));
	l->setBuddy(m_toAddr);
	grid->addWidget(l, 4, 0);
	grid->addWidget(m_toAddr, 4, 1);
	grid->setRowStretch(5, 1);

	connect(m_port, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotChanged()));
	connect(m_toPort, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotChanged()));
	connect(m_toAddr, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotChanged()));
	m_port->setFocus();
	slotChanged();
	setMinimumWidth(380);
}

void ForwardPortDialog::slotChanged()
{
	bool ok = PortDialog::validPort(m_port->text());
	if (!m_toPort->text().stripWhiteSpace().isEmpty())
		ok = ok && PortDialog::validPort(m_toPort->text());
	/* at least one destination must be given */
	ok = ok && (!m_toPort->text().stripWhiteSpace().isEmpty() ||
	            !m_toAddr->text().stripWhiteSpace().isEmpty());
	enableButtonOK(ok);
}

void ForwardPortDialog::slotOk()
{
	KDialogBase::slotOk();
}

FwForwardPort ForwardPortDialog::forwardPort() const
{
	FwForwardPort fp;
	fp.port = m_port->text().stripWhiteSpace();
	fp.protocol = m_protocol->currentText();
	fp.toPort = m_toPort->text().stripWhiteSpace();
	fp.toAddr = m_toAddr->text().stripWhiteSpace();
	return fp;
}

/* ---------------------------------------------------------------------- */

static const char *richRuleExamples[] = {
	"rule family=\"ipv4\" source address=\"192.168.1.0/24\" service name=\"ssh\" accept",
	"rule family=\"ipv4\" source address=\"10.0.0.5\" reject",
	"rule service name=\"ftp\" log prefix=\"ftp\" level=\"info\" limit value=\"1/m\" accept",
	"rule family=\"ipv4\" source address=\"192.168.1.0/24\" port port=\"3306\" protocol=\"tcp\" accept",
	"rule family=\"ipv6\" source address=\"2001:db8::/32\" drop",
	0
};

RichRuleDialog::RichRuleDialog(TQWidget *parent)
	: KDialogBase(parent, "RichRuleDialog", true, i18n("Add Rich Rule"), Ok | Cancel, Ok, true)
{
	TQWidget *page = new TQWidget(this);
	setMainWidget(page);
	TQVBoxLayout *vbox = new TQVBoxLayout(page, 0, spacingHint());

	TQLabel *intro = new TQLabel(i18n("Rich rules use the firewalld rich language "
	                                  "(see <i>man firewalld.richlanguage</i>)."), page);
	intro->setAlignment(TQt::WordBreak);
	vbox->addWidget(intro);

	TQHBoxLayout *hbox = new TQHBoxLayout(vbox);
	TQLabel *l = new TQLabel(i18n("&Example:"), page);
	m_examples = new KComboBox(false, page);
	m_examples->insertItem(i18n("Choose an example to start from..."));
	for (int i = 0; richRuleExamples[i]; ++i)
		m_examples->insertItem(TQString::fromLatin1(richRuleExamples[i]));
	/* the examples are long; do not let them widen the dialog */
	m_examples->setSizePolicy(TQSizePolicy(TQSizePolicy::Ignored, TQSizePolicy::Fixed));
	l->setBuddy(m_examples);
	hbox->addWidget(l);
	hbox->addWidget(m_examples, 1);

	l = new TQLabel(i18n("&Rule:"), page);
	m_rule = new TQTextEdit(page);
	m_rule->setTextFormat(TQt::PlainText);
	m_rule->setWordWrap(TQTextEdit::WidgetWidth);
	l->setBuddy(m_rule);
	vbox->addWidget(l);
	vbox->addWidget(m_rule, 1);

	connect(m_rule, TQ_SIGNAL(textChanged()), TQ_SLOT(slotChanged()));
	connect(m_examples, TQ_SIGNAL(activated(int)), TQ_SLOT(slotExample(int)));
	m_rule->setFocus();
	slotChanged();
	resize(620, 300);
}

void RichRuleDialog::slotExample(int index)
{
	if (index > 0)
		m_rule->setText(m_examples->text(index));
}

void RichRuleDialog::slotChanged()
{
	enableButtonOK(rule().startsWith("rule"));
}

TQString RichRuleDialog::rule() const
{
	return m_rule->text().simplifyWhiteSpace();
}

/* ---------------------------------------------------------------------- */

EntryDialog::EntryDialog(TQWidget *parent, const TQString &caption, const TQString &label,
                         const TQString &whatsThis, const TQStringList &suggestions)
	: KDialogBase(parent, "EntryDialog", true, caption, Ok | Cancel, Ok, true)
{
	TQWidget *page = new TQWidget(this);
	setMainWidget(page);
	TQVBoxLayout *vbox = new TQVBoxLayout(page, 0, spacingHint());

	TQLabel *l = new TQLabel(label, page);
	m_entry = new KComboBox(true, page);
	m_entry->insertStringList(suggestions);
	m_entry->setCurrentText(TQString::null);
	l->setBuddy(m_entry);
	if (!whatsThis.isEmpty())
		TQWhatsThis::add(m_entry, whatsThis);
	vbox->addWidget(l);
	vbox->addWidget(m_entry);
	vbox->addStretch(1);

	connect(m_entry, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotChanged()));
	m_entry->setFocus();
	slotChanged();
	setMinimumWidth(340);
}

void EntryDialog::slotChanged()
{
	enableButtonOK(!text().isEmpty() && text().find(' ') < 0);
}

TQString EntryDialog::text() const
{
	return m_entry->currentText().stripWhiteSpace();
}

/* ---------------------------------------------------------------------- */

ZoneChooserDialog::ZoneChooserDialog(TQWidget *parent, const TQString &iface,
                                     const TQStringList &zones, const TQString &current,
                                     bool allowDefault)
	: KDialogBase(parent, "ZoneChooserDialog", true, i18n("Change Zone"), Ok | Cancel, Ok, true),
	  m_allowDefault(allowDefault)
{
	TQWidget *page = new TQWidget(this);
	setMainWidget(page);
	TQVBoxLayout *vbox = new TQVBoxLayout(page, 0, spacingHint());

	TQLabel *l = new TQLabel(i18n("Select the zone for interface <b>%1</b>:").arg(iface), page);
	m_zones = new KComboBox(false, page);
	if (allowDefault)
		m_zones->insertItem(i18n("(default zone)"));
	m_zones->insertStringList(zones);
	l->setBuddy(m_zones);
	vbox->addWidget(l);
	vbox->addWidget(m_zones);
	vbox->addStretch(1);

	int idx = zones.findIndex(current);
	if (idx >= 0)
		m_zones->setCurrentItem(idx + (allowDefault ? 1 : 0));
	setMinimumWidth(300);
}

TQString ZoneChooserDialog::zone() const
{
	if (m_allowDefault && m_zones->currentItem() == 0)
		return TQString::null;
	return m_zones->currentText();
}

/* ---------------------------------------------------------------------- */

bool confirmPanicMode(TQWidget *parent)
{
	int res = KMessageBox::warningContinueCancel(parent,
		i18n("Panic mode blocks <b>all</b> incoming and outgoing network traffic "
		     "and terminates active connections.<br><br>Do you really want to enable it?"),
		i18n("Enable Panic Mode"), KGuiItem(i18n("Enable Panic Mode"), "process-stop"));
	return res == KMessageBox::Continue;
}

#include "dialogs.moc"
