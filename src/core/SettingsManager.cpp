/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2016 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2014, 2016 Piotr Wójcik <chocimier@tlen.pl>
* Copyright (C) 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "SettingsManager.h"

#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>

namespace Otter
{

SettingsManager* SettingsManager::m_instance = NULL;
QString SettingsManager::m_globalPath;
QString SettingsManager::m_overridePath;
QHash<QString, QVariant> SettingsManager::m_defaults;

SettingsManager::SettingsManager(QObject *parent) : QObject(parent)
{
}

void SettingsManager::createInstance(const QString &path, QObject *parent)
{
	if (!m_instance)
	{
		m_instance = new SettingsManager(parent);
		m_globalPath = path + QLatin1String("/otter.conf");
		m_overridePath = path + QLatin1String("/override.ini");

		QSettings defaults(QLatin1String(":/schemas/options.ini"), QSettings::IniFormat);
		const QStringList groups(defaults.childGroups());

		for (int i = 0; i < groups.count(); ++i)
		{
			defaults.beginGroup(groups.at(i));

			const QStringList keys(defaults.childGroups());

			for (int j = 0; j < keys.count(); ++j)
			{
				m_defaults[QStringLiteral("%1/%2").arg(groups.at(i)).arg(keys.at(j))] = defaults.value(QStringLiteral("%1/value").arg(keys.at(j)));
			}

			defaults.endGroup();
		}

		m_defaults[QLatin1String("Paths/Downloads")] = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
		m_defaults[QLatin1String("Paths/SaveFile")] = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
	}
}

void SettingsManager::removeOverride(const QUrl &url, const QString &key)
{
	if (key.isEmpty())
	{
		QSettings(m_overridePath, QSettings::IniFormat).remove(getHost(url));
	}
	else
	{
		QSettings(m_overridePath, QSettings::IniFormat).remove(getHost(url) + QLatin1Char('/') + key);
	}
}

void SettingsManager::setValue(const QString &key, const QVariant &value, const QUrl &url)
{
	if (!url.isEmpty())
	{
		if (value.isNull())
		{
			QSettings(m_overridePath, QSettings::IniFormat).remove(getHost(url) + QLatin1Char('/') + key);
		}
		else
		{
			QSettings(m_overridePath, QSettings::IniFormat).setValue(getHost(url) + QLatin1Char('/') + key, value);
		}

		emit m_instance->valueChanged(key, value, url);

		return;
	}

	if (getValue(key) != value)
	{
		QSettings(m_globalPath, QSettings::IniFormat).setValue(key, value);

		emit m_instance->valueChanged(key, value);
	}
}

SettingsManager* SettingsManager::getInstance()
{
	return m_instance;
}

QString SettingsManager::getHost(const QUrl &url)
{
	return (url.isLocalFile() ? QLatin1String("localhost") : url.host());
}

QString SettingsManager::getReport()
{
	QString report;
	QTextStream stream(&report);
	stream.setFieldAlignment(QTextStream::AlignLeft);
	stream << QLatin1String("Settings:\n");

	QStringList excludeValues;
	QSettings defaults(QLatin1String(":/schemas/options.ini"), QSettings::IniFormat);
	const QStringList defaultsGroups(defaults.childGroups());

	for (int i = 0; i < defaultsGroups.count(); ++i)
	{
		defaults.beginGroup(defaultsGroups.at(i));

		const QStringList keys(defaults.childGroups());

		for (int j = 0; j < keys.count(); ++j)
		{
			const QString type(defaults.value(QStringLiteral("%1/type").arg(keys.at(j))).toString());

			if (type == QLatin1String("string") || type == QLatin1String("path"))
			{
				excludeValues.append(QStringLiteral("%1/%2").arg(defaultsGroups.at(i)).arg(keys.at(j)));
			}
		}

		defaults.endGroup();
	}

	QHash<QString, int> overridenValues;
	QSettings overrides(m_overridePath, QSettings::IniFormat);
	const QStringList overridesGroups(overrides.childGroups());

	for (int i = 0; i < overridesGroups.count(); ++i)
	{
		overrides.beginGroup(overridesGroups.at(i));

		const QStringList keys(overrides.allKeys());

		for (int j = 0; j < keys.count(); ++j)
		{
			if (overridenValues.contains(keys.at(j)))
			{
				++overridenValues[keys.at(j)];
			}
			else
			{
				overridenValues[keys.at(j)] = 1;
			}
		}

		overrides.endGroup();
	}

	QStringList keys(m_defaults.keys());
	keys.sort();

	for (int i = 0; i < keys.count(); ++i)
	{
		stream << QLatin1Char('\t');
		stream.setFieldWidth(50);
		stream << keys.at(i);
		stream.setFieldWidth(20);

		if (excludeValues.contains(keys.at(i)))
		{
			stream << QLatin1Char('-');
		}
		else
		{
			stream << m_defaults[keys.at(i)].toString();
		}

		stream << ((m_defaults[keys.at(i)] == getValue(keys.at(i))) ? QLatin1String("default") : QLatin1String("non default"));
		stream << (overridenValues.contains(keys.at(i)) ? QStringLiteral("%1 override(s)").arg(overridenValues[keys.at(i)]) : QLatin1String("no overrides"));
		stream.setFieldWidth(0);
		stream << QLatin1Char('\n');
	}

	stream << QLatin1Char('\n');

	return report;
}

QVariant SettingsManager::getValue(const QString &key, const QUrl &url)
{
	if (!url.isEmpty())
	{
		return QSettings(m_overridePath, QSettings::IniFormat).value(getHost(url) + QLatin1Char('/') + key, getValue(key));
	}

	return QSettings(m_globalPath, QSettings::IniFormat).value(key, m_defaults[key]);
}

QStringList SettingsManager::getOptions()
{
	QStringList options;
	QSettings settings(QLatin1String(":/schemas/options.ini"), QSettings::IniFormat);
	const QStringList groups(settings.childGroups());

	for (int i = 0; i < groups.count(); ++i)
	{
		settings.beginGroup(groups.at(i));

		const QStringList children(settings.childGroups());

		for (int j = 0; j < children.count(); ++j)
		{
			settings.beginGroup(children.at(j));

			options.append(settings.group());

			settings.endGroup();
		}

		settings.endGroup();
	}

	return options;
}

SettingsManager::OptionDefinition SettingsManager::getDefinition(const QString &key)
{
	QSettings settings(QLatin1String(":/schemas/options.ini"), QSettings::IniFormat);
	settings.beginGroup(key);

	OptionDefinition options;
	options.name = key;
	options.defaultValue = m_defaults[key];

	const QString type(settings.value(QLatin1String("type")).toString());

	if (type == QLatin1String("bool"))
	{
		options.type = BooleanType;
	}
	else if (type == QLatin1String("color"))
	{
		options.type = ColorType;
	}
	else if (type == QLatin1String("enumeration"))
	{
		options.type = EnumerationType;
		options.choices = settings.value(QLatin1String("choices")).toStringList();
	}
	else if (type == QLatin1String("font"))
	{
		options.type = FontType;
	}
	else if (type == QLatin1String("icon"))
	{
		options.type = IconType;
	}
	else if (type == QLatin1String("integer"))
	{
		options.type = IntegerType;
	}
	else if (type == QLatin1String("list"))
	{
		options.type = ListType;
	}
	else if (type == QLatin1String("path"))
	{
		options.type = PathType;
	}
	else if (type == QLatin1String("string"))
	{
		options.type = StringType;
	}
	else
	{
		options.type = UnknownType;
	}

	return options;
}

bool SettingsManager::hasOverride(const QUrl &url, const QString &key)
{
	if (key.isEmpty())
	{
		return QSettings(m_overridePath, QSettings::IniFormat).childGroups().contains(getHost(url));
	}
	else
	{
		return QSettings(m_overridePath, QSettings::IniFormat).contains(getHost(url) + QLatin1Char('/') + key);
	}
}

}
