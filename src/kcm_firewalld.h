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

#ifndef KCM_FIREWALLD_H
#define KCM_FIREWALLD_H

#include <tdecmodule.h>

class FirewallWidget;

class KCMFirewalld : public TDECModule
{
	TQ_OBJECT
public:
	KCMFirewalld(TQWidget *parent = 0, const char *name = 0, const TQStringList &args = TQStringList());
	virtual ~KCMFirewalld();

	virtual void load();
	virtual TQString quickHelp() const;

private:
	FirewallWidget *m_widget;
};

#endif
