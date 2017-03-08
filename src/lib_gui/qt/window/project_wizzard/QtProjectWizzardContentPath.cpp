#include "qt/window/project_wizzard/QtProjectWizzardContentPath.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "component/view/DialogView.h"
#include "qt/element/QtLocationPicker.h"
#include "settings/ApplicationSettings.h"
#include "settings/JavaProjectSettings.h"
#include "utility/file/FileManager.h"
#include "utility/messaging/type/MessageStatus.h"
#include "utility/utilityMaven.h"
#include "utility/ScopedFunctor.h"
#include "Application.h"

QtProjectWizzardContentPath::QtProjectWizzardContentPath(std::shared_ptr<ProjectSettings> settings, QtProjectWizzardWindow* window)
	: QtProjectWizzardContent(settings, window)
	, m_makePathRelativeToProjectFileLocation(true)
{
}

void QtProjectWizzardContentPath::populate(QGridLayout* layout, int& row)
{
	QLabel* label = createFormLabel(m_titleString);
	layout->addWidget(label, row, QtProjectWizzardWindow::FRONT_COL, Qt::AlignTop);

	if (m_helpString.size() > 0)
	{
		addHelpButton(m_helpString, layout, row);
	}

	m_picker = new QtLocationPicker(this);
	m_picker->setPickDirectory(true);

	if (m_makePathRelativeToProjectFileLocation)
	{
		m_picker->setRelativeRootDirectory(m_settings->getProjectFileLocation());
	}

	layout->addWidget(m_picker, row, QtProjectWizzardWindow::BACK_COL);
	row++;
}

bool QtProjectWizzardContentPath::check()
{
	if (m_picker->getText().isEmpty())
	{
		QMessageBox msgBox;
		msgBox.setText("Please define the location for the \"" + m_titleString + "\".");
		msgBox.exec();
		return false;
	}

	return true;
}

void QtProjectWizzardContentPath::setTitleString(const QString& title)
{
	m_titleString = title;
}

void QtProjectWizzardContentPath::setHelpString(const QString& help)
{
	m_helpString = help;
}

QtProjectWizzardContentPathSourceMaven::QtProjectWizzardContentPathSourceMaven(
	std::shared_ptr<ProjectSettings> settings, QtProjectWizzardWindow* window
)
	: QtProjectWizzardContentPath(settings, window)
{
	setTitleString("Maven Project Root");
	setHelpString(
		"Enter the root folder of your Maven project (where the main .pom file resides).<br />"
		"<br />"
		"You can make use of environment variables with ${ENV_VAR}."
	);
}

void QtProjectWizzardContentPathSourceMaven::populate(QGridLayout* layout, int& row)
{
	QtProjectWizzardContentPath::populate(layout, row);
	m_picker->setPickDirectory(false);
	m_picker->setFileFilter("POM File (pom.xml)");

	QPushButton* filesButton = addFilesButton("show source files", nullptr, row);
	m_shouldIndexTests = new QCheckBox("Should Index Tests");

	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->addWidget(m_shouldIndexTests);
	hlayout->addStretch();
	hlayout->addWidget(filesButton);

	layout->addLayout(hlayout, row, QtProjectWizzardWindow::BACK_COL);
	row++;
}

void QtProjectWizzardContentPathSourceMaven::load()
{
	std::shared_ptr<JavaProjectSettings> javaSettings = std::dynamic_pointer_cast<JavaProjectSettings>(m_settings);
	if (javaSettings)
	{
		m_picker->setText(QString::fromStdString(javaSettings->getMavenProjectFilePath().str()));
		m_shouldIndexTests->setChecked(javaSettings->getShouldIndexMavenTests());
	}
}

void QtProjectWizzardContentPathSourceMaven::save()
{
	std::shared_ptr<JavaProjectSettings> javaSettings = std::dynamic_pointer_cast<JavaProjectSettings>(m_settings);
	if (javaSettings)
	{
		javaSettings->setMavenProjectFilePath(m_picker->getText().toStdString());
		javaSettings->setShouldIndexMavenTests(m_shouldIndexTests->isChecked());
	}
}

std::vector<std::string> QtProjectWizzardContentPathSourceMaven::getFileNames() const
{
	std::shared_ptr<JavaProjectSettings> javaSettings = std::dynamic_pointer_cast<JavaProjectSettings>(m_settings);

	const FilePath mavenPath = ApplicationSettings::getInstance()->getMavenPath();
	const FilePath mavenProjectRoot = javaSettings->getAbsoluteMavenProjectFilePath().parentDirectory();

	std::vector<std::string> list;

	const bool success = utility::mavenGenerateSources(mavenPath, mavenProjectRoot);
	if (!success)
	{
		const std::string dialogMessage =
			"Coati was unable to locate Maven on this machine.\n"
			"Please make sure to provide the correct Maven Path in the preferences.";

		MessageStatus(dialogMessage, true, false).dispatch();

		Application::getInstance()->handleDialog(dialogMessage);
	}
	else
	{
		const std::vector<FilePath> sourceDirectories = utility::mavenGetAllDirectoriesFromEffectivePom(
			mavenPath,
			mavenProjectRoot,
			javaSettings->getShouldIndexMavenTests()
		);

		FileManager fileManager;
		fileManager.setPaths(
			sourceDirectories,
			std::vector<FilePath>(), // we don't need to specify header paths here
			m_settings->getAbsoluteExcludePaths(),
			m_settings->getSourceExtensions()
		);

		const FilePath projectPath = m_settings->getProjectFileLocation();

		for (FilePath path: fileManager.getSourceFilePaths())
		{
			if (projectPath.exists())
			{
				path = path.relativeTo(projectPath);
			}

			list.push_back(path.str());
		}
	}

	return list;
}

QtProjectWizzardContentPathDependenciesMaven::QtProjectWizzardContentPathDependenciesMaven(
	std::shared_ptr<ProjectSettings> settings, QtProjectWizzardWindow* window
)
	: QtProjectWizzardContentPath(settings, window)
{
	setTitleString("Intermediate Dependencies Directory");
	setHelpString(
		"This directory is used to temporarily download and store the dependencies (e.g. .jar files) of the Maven project while it is indexed.<br />"
		"<br />"
		"You can make use of environment variables with ${ENV_VAR}."
	);
}

void QtProjectWizzardContentPathDependenciesMaven::load()
{
	std::shared_ptr<JavaProjectSettings> javaSettings = std::dynamic_pointer_cast<JavaProjectSettings>(m_settings);
	if (javaSettings)
	{
		m_picker->setText(QString::fromStdString(javaSettings->getMavenDependenciesDirectory().str()));
	}
}

void QtProjectWizzardContentPathDependenciesMaven::save()
{
	std::shared_ptr<JavaProjectSettings> javaSettings = std::dynamic_pointer_cast<JavaProjectSettings>(m_settings);
	if (javaSettings)
	{
		javaSettings->setMavenDependenciesDirectory(m_picker->getText().toStdString());
	}
}
