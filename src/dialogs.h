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

#ifndef DIALOGS_H
#define DIALOGS_H

#include <kdialogbase.h>

#include "firewalldclient.h"

class KLineEdit;
class KComboBox;
class TQTextEdit;

/** Ask for a port (or port range) and a protocol */
class PortDialog : public KDialogBase
{
	TQ_OBJECT
public:
	PortDialog(TQWidget *parent, const TQString &caption);
	FwPort port() const;

	static bool validPort(const TQString &port);

protected slots:
	void slotOk();
	void slotChanged();

private:
	KLineEdit *m_port;
	KComboBox *m_protocol;
};

/** Ask for a port forwarding definition */
class ForwardPortDialog : public KDialogBase
{
	TQ_OBJECT
public:
	ForwardPortDialog(TQWidget *parent);
	FwForwardPort forwardPort() const;

protected slots:
	void slotOk();
	void slotChanged();

private:
	KLineEdit *m_port;
	KComboBox *m_protocol;
	KLineEdit *m_toPort;
	KLineEdit *m_toAddr;
};

/** Edit a firewalld rich rule */
class RichRuleDialog : public KDialogBase
{
	TQ_OBJECT
public:
	RichRuleDialog(TQWidget *parent);
	TQString rule() const;

protected slots:
	void slotChanged();
	void slotExample(int index);

private:
	TQTextEdit *m_rule;
	KComboBox *m_examples;
};

/** Ask for a single text entry (interface, source address...) */
class EntryDialog : public KDialogBase
{
	TQ_OBJECT
public:
	EntryDialog(TQWidget *parent, const TQString &caption, const TQString &label,
	            const TQString &whatsThis, const TQStringList &suggestions = TQStringList());
	TQString text() const;

protected slots:
	void slotChanged();

private:
	KComboBox *m_entry;
};

/** Choose a zone for a network interface */
class ZoneChooserDialog : public KDialogBase
{
	TQ_OBJECT
public:
	ZoneChooserDialog(TQWidget *parent, const TQString &iface, const TQStringList &zones,
	                  const TQString &current, bool allowDefault);
	/** empty string means "use the default zone" */
	TQString zone() const;

private:
	KComboBox *m_zones;
	bool m_allowDefault;
};

#endif
