#include "qt/window/project_wizzard/QtProjectWizzard.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSysInfo>

#include "qt/window/project_wizzard/QtProjectWizzardContent.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentBuildFile.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentCDBSource.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentData.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentExtensions.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentFlags.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentPath.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentPaths.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentPreferences.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentSimple.h"
#include "qt/window/project_wizzard/QtProjectWizzardContentSummary.h"
#include "qt/window/project_wizzard/QtProjectWizzardWindow.h"
#include "settings/CxxProjectSettings.h"
#include "settings/JavaProjectSettings.h"
#include "utility/file/FileSystem.h"
#include "utility/logging/logging.h"
#include "utility/messaging/type/MessageLoadProject.h"
#include "utility/messaging/type/MessagePluginPortChange.h"
#include "utility/messaging/type/MessageRefresh.h"
#include "utility/messaging/type/MessageScrollSpeedChange.h"
#include "utility/messaging/type/MessageStatus.h"
#include "utility/utility.h"
#include "utility/utilityPathDetection.h"
#include "utility/utilityString.h"

#include "Application.h"

QtProjectWizzard::QtProjectWizzard(QWidget* parent)
	: QtWindowStackElement(parent)
	, m_windowStack(this)
	, m_editing(false)
{
	connect(&m_windowStack, SIGNAL(push()), this, SLOT(windowStackChanged()));
	connect(&m_windowStack, SIGNAL(pop()), this, SLOT(windowStackChanged()));

	// save old application settings so they can be compared later
	ApplicationSettings* appSettings = ApplicationSettings::getInstance().get();
	m_appSettings.setHeaderSearchPaths(appSettings->getHeaderSearchPaths());
	m_appSettings.setFrameworkSearchPaths(appSettings->getFrameworkSearchPaths());
	m_appSettings.setScrollSpeed(appSettings->getScrollSpeed());
	m_appSettings.setCoatiPort(appSettings->getCoatiPort());
	m_appSettings.setPluginPort(appSettings->getPluginPort());

	m_parserManager = std::make_shared<SolutionParserManager>();

}

void QtProjectWizzard::showWindow()
{
	QtWindowStackElement* element = m_windowStack.getTopWindow();

	if (element)
	{
		element->show();
	}
}

void QtProjectWizzard::hideWindow()
{
	QtWindowStackElement* element = m_windowStack.getTopWindow();

	if (element)
	{
		element->hide();
	}
}

void QtProjectWizzard::newProject()
{
	QtProjectWizzardWindow* window = createWindowWithContent<QtProjectWizzardContentSelect>();

	connect(dynamic_cast<QtProjectWizzardContentSelect*>(window->content()),
		SIGNAL(selected(ProjectType)),
		this, SLOT(selectedProjectType(ProjectType)));

	window->setNextEnabled(false);
	window->setPreviousEnabled(false);
	window->updateSubTitle("Type Selection");
}

void QtProjectWizzard::newProjectFromSolution(const std::string& ideId, const std::string& solutionPath)
{
	if (m_parserManager->canParseSolution(ideId))
	{
		std::shared_ptr<ProjectSettings> settings = m_parserManager->getProjectSettings(ideId, solutionPath);

		// this looks rather hacky.. calling the edit project and then setting the title.
		editProject(settings);

		QWidget* widget = m_windowStack.getTopWindow();

		if (widget)
		{
			QtProjectWizzardWindow* window = dynamic_cast<QtProjectWizzardWindow*>(widget);

			std::string title = "NEW PROJECT FROM ";
			title += utility::toUpperCase(ideId);
			title += " SOLUTION";

			window->updateTitle(QString(title.c_str()));
			window->updateNextButton("Create");
			window->setPreviousVisible(m_windowStack.getWindowCount() > 0);
		}
	}
	else
	{
		LOG_ERROR_STREAM(<< "Unknown solution type");
		MessageStatus("Unable to parse given solution", true).dispatch();
	}
}

void QtProjectWizzard::newProjectFromCDB(const std::string& filePath, const std::vector<std::string>& headerPaths)
{
	FilePath fp(filePath);

	std::vector<FilePath> hp;
	for (size_t i = 0; i < headerPaths.size(); i++)
	{
		hp.push_back(FilePath(headerPaths[i]));
	}

	std::shared_ptr<CxxProjectSettings> settings = std::make_shared<CxxProjectSettings>();
	settings->setLanguage(LanguageType::LANGUAGE_CPP);
	settings->setProjectName(fp.withoutExtension().fileName());
	settings->setProjectFileLocation(fp.parentDirectory());
	settings->setCompilationDatabasePath(fp);
	settings->setSourcePaths(hp);
	m_settings = settings;
	emptyProjectCDBVS();
}

void QtProjectWizzard::refreshProjectFromSolution(const std::string& ideId, const std::string& solutionPath)
{
	if (m_parserManager->canParseSolution(ideId))
	{
		QtProjectWizzardWindow* window = dynamic_cast<QtProjectWizzardWindow*>(m_windowStack.getTopWindow());
		if (window)
		{
			window->content()->save();
		}

		std::shared_ptr<ProjectSettings> settings = m_parserManager->getProjectSettings(ideId, solutionPath);

		std::shared_ptr<CxxProjectSettings> otherCxxSettings = std::dynamic_pointer_cast<CxxProjectSettings>(settings);
		std::shared_ptr<CxxProjectSettings> ownCxxSettings = std::dynamic_pointer_cast<CxxProjectSettings>(m_settings);
		if (otherCxxSettings && ownCxxSettings)
		{
			ownCxxSettings->setSourcePaths(otherCxxSettings->getSourcePaths());
			ownCxxSettings->setHeaderSearchPaths(otherCxxSettings->getHeaderSearchPaths());
			ownCxxSettings->setVisualStudioSolutionPath(FilePath(solutionPath));
		}
		if (window)
		{
			window->content()->load();
		}
	}
	else
	{
		LOG_ERROR_STREAM(<< "Unknown solution type");
		MessageStatus("Unable to parse given solution", true).dispatch();
	}
}

void QtProjectWizzard::editProject(const FilePath& settingsPath)
{
	std::shared_ptr<ProjectSettings> settings;
	switch (ProjectSettings::getLanguageOfProject(settingsPath))
	{
	case LANGUAGE_C:
	case LANGUAGE_CPP:
		settings = std::make_shared<CxxProjectSettings>(settingsPath);
		break;
	case LANGUAGE_JAVA:
		settings = std::make_shared<JavaProjectSettings>(settingsPath);
		break;
	default:
		return;
	}

	if (settings)
	{
		settings->reload();
		editProject(settings);
	}
}

void QtProjectWizzard::editProject(std::shared_ptr<ProjectSettings> settings)
{
	m_settings = settings;
	m_editing = true;

	switch (m_settings->getLanguage())
	{
		case LANGUAGE_JAVA:
			showSummaryJava();
			break;

		case LANGUAGE_C:
		case LANGUAGE_CPP:
			showSummary();
			break;

		default:
			break;
	}
}

void QtProjectWizzard::showPreferences()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->setIsForm(true);

			summary->addContent(new QtProjectWizzardContentPreferences(m_settings, window));

			summary->addContent(new QtProjectWizzardContentPathsHeaderSearchGlobal(m_settings, window));
			if (QSysInfo::macVersion() != QSysInfo::MV_None)
			{
				summary->addContent(new QtProjectWizzardContentPathsFrameworkSearchGlobal(m_settings, window));
			}

			window->setup();

			window->updateTitle("PREFERENCES");
			window->updateNextButton("Save");
			window->setPreviousVisible(false);
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(savePreferences()));
}

bool QtProjectWizzard::applicationSettingsContainVisualStudioHeaderSearchPaths()
{
	std::vector<FilePath> expandedPaths;
	const std::shared_ptr<CombinedPathDetector> headerPathDetector = utility::getCxxVsHeaderPathDetector();
	for (const std::string& detectorName: headerPathDetector->getWorkingDetectorNames())
	{
		for (const FilePath& path: headerPathDetector->getPaths(detectorName))
		{
			utility::append(expandedPaths, path.expandEnvironmentVariables());
		}
	}

	std::vector<FilePath> usedExpandedGlobalHeaderSearchPaths = ApplicationSettings::getInstance()->getHeaderSearchPathsExpanded();
	for (const FilePath& usedExpandedPath: usedExpandedGlobalHeaderSearchPaths)
	{
		for (const FilePath& expandedPath: expandedPaths)
		{
			if (expandedPath == usedExpandedPath)
			{
				return true;
			}
		}
	}

	return false;
}

template<typename T>
QtProjectWizzardWindow* QtProjectWizzard::createWindowWithContent()
{
	QtProjectWizzardWindow* window = new QtProjectWizzardWindow(parentWidget());

	connect(window, SIGNAL(previous()), &m_windowStack, SLOT(popWindow()));
	connect(window, SIGNAL(canceled()), this, SLOT(cancelWizzard()));

	window->setContent(new T(m_settings, window));
	window->setPreferredSize(QSize(580, 340));
	window->setup();

	m_windowStack.pushWindow(window);

	return window;
}

template<>
QtProjectWizzardWindow* QtProjectWizzard::createWindowWithContent<QtProjectWizzardContentSelect>()
{
	QtProjectWizzardWindow* window = new QtProjectWizzardWindow(parentWidget());

	connect(window, SIGNAL(previous()), &m_windowStack, SLOT(popWindow()));
	connect(window, SIGNAL(canceled()), this, SLOT(cancelWizzard()));

	QtProjectWizzardContentSelect* content = new QtProjectWizzardContentSelect(m_settings, window, m_parserManager);

	window->setContent(content);
	window->setPreferredSize(QSize(570, 380));
	window->setup();

	m_windowStack.pushWindow(window);

	return window;
}

QtProjectWizzardWindow* QtProjectWizzard::createWindowWithSummary(
	std::function<void(QtProjectWizzardWindow*, QtProjectWizzardContentSummary*)> func
){
	QtProjectWizzardWindow* window = new QtProjectWizzardWindow(parentWidget());

	connect(window, SIGNAL(previous()), &m_windowStack, SLOT(popWindow()));
	connect(window, SIGNAL(canceled()), this, SLOT(cancelWizzard()));

	QtProjectWizzardContentSummary* summary = new QtProjectWizzardContentSummary(m_settings, window);

	window->setContent(summary);
	window->setPreferredSize(QSize(750, 500));
	func(window, summary);

	m_windowStack.pushWindow(window);

	return window;
}

void QtProjectWizzard::cancelWizzard()
{
	m_windowStack.clearWindows();
	emit canceled();
}

void QtProjectWizzard::finishWizzard()
{
	m_windowStack.clearWindows();
	emit finished();
}

void QtProjectWizzard::windowStackChanged()
{
	QWidget* window = m_windowStack.getTopWindow();

	if (window)
	{
		dynamic_cast<QtProjectWizzardWindow*>(window)->content()->load();
	}
}

void QtProjectWizzard::selectedProjectType(ProjectType projectType)
{
	LanguageType languageType = getLanguageTypeForProjectType(projectType);
	switch (projectType)
	{
	case PROJECT_C_EMPTY:
	case PROJECT_CPP_EMPTY:
		m_settings = std::make_shared<CxxProjectSettings>();
		if (applicationSettingsContainVisualStudioHeaderSearchPaths())
		{
			std::vector<std::string> flags;
			flags.push_back("-fms-extensions");
			flags.push_back("-fms-compatibility");
			flags.push_back("-fms-compatibility-version=19");
			std::dynamic_pointer_cast<CxxProjectSettings>(m_settings)->setCompilerFlags(flags);
		}
		m_settings->setLanguage(languageType);
		emptyProject();
		break;
	case PROJECT_CXX_CDB:
		m_settings = std::make_shared<CxxProjectSettings>();
		m_settings->setLanguage(languageType);
		emptyProjectCDB();
		break;
	case PROJECT_CXX_VS:
		m_settings = std::make_shared<CxxProjectSettings>();
		m_settings->setLanguage(languageType);
		emptyProjectCDBVS();
		break;
	case PROJECT_JAVA_EMPTY:
		m_settings = std::make_shared<JavaProjectSettings>();
		m_settings->setLanguage(languageType);
		emptyProject();
		break;
	case PROJECT_JAVA_MAVEN:
		m_settings = std::make_shared<JavaProjectSettings>();
		m_settings->setLanguage(languageType);
		emptyProjectJavaMaven();
		break;
	}
}

void QtProjectWizzard::emptyProject()
{
	QtProjectWizzardWindow* window = createWindowWithContent<QtProjectWizzardContentData>();
	window->updateSubTitle("Project Data");

	if (m_settings->getLanguage() == LANGUAGE_JAVA)
	{
		connect(window, SIGNAL(next()), this, SLOT(sourcePathsJava()));
	}
	else
	{
		connect(window, SIGNAL(next()), this, SLOT(sourcePaths()));
	}
}

void QtProjectWizzard::sourcePaths()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->addContent(new QtProjectWizzardContentPathsSource(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentExtensions(m_settings, window));

			window->setup();
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(headerSearchPaths()));
	window->updateSubTitle("Indexed Paths");
}

void QtProjectWizzard::headerSearchPaths()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->addContent(new QtProjectWizzardContentPathsHeaderSearch(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentPathsHeaderSearchGlobal(m_settings, window));

			window->setup();
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(headerSearchPathsDone()));
	window->updateSubTitle("Include Paths");
}

void QtProjectWizzard::headerSearchPathsDone()
{
	if (QSysInfo::macVersion() != QSysInfo::MV_None)
	{
		frameworkSearchPaths();
	}
	else
	{
		advancedSettingsCxx();
	}
}

void QtProjectWizzard::frameworkSearchPaths()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->addContent(new QtProjectWizzardContentPathsFrameworkSearch(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentPathsFrameworkSearchGlobal(m_settings, window));

			window->setup();
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(advancedSettingsCxx()));
	window->updateSubTitle("Framework Search Paths");
}

void QtProjectWizzard::emptyProjectCDBVS()
{
	QtProjectWizzardWindow* window = createWindowWithContent<QtProjectWizzardContentDataCDBVS>();

	connect(window, SIGNAL(next()), this, SLOT(headerPathsCDB()));
	window->updateSubTitle("Project Data");
}

void QtProjectWizzard::emptyProjectCDB()
{
	QtProjectWizzardWindow* window = createWindowWithContent<QtProjectWizzardContentDataCDB>();

	connect(window, SIGNAL(next()), this, SLOT(headerPathsCDB()));
	window->updateSubTitle("Project Data");
}

void QtProjectWizzard::emptyProjectJavaMaven()
{
	QtProjectWizzardWindow* window = createWindowWithContent<QtProjectWizzardContentData>();

	connect(window, SIGNAL(next()), this, SLOT(sourcePathsJavaMaven()));
	window->updateSubTitle("Project Data");
}

void QtProjectWizzard::headerPathsCDB()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->addContent(new QtProjectWizzardContentCDBSource(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentPathsCDBHeader(m_settings, window));

			window->setup();
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(advancedSettingsCxx()));
	window->updateSubTitle("Indexed Header Paths");
}

void QtProjectWizzard::sourcePathsJava()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->addContent(new QtProjectWizzardContentPathsSource(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentPathsClassJava(m_settings, window));

			window->setup();
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(advancedSettingsJava()));
	window->updateSubTitle("Indexed Paths");
}

void QtProjectWizzard::sourcePathsJavaMaven()
{
	std::dynamic_pointer_cast<JavaProjectSettings>(m_settings)->setMavenDependenciesDirectory(
		"./coati_dependencies/" + utility::replace(m_settings->getProjectName(), " ", "_") + "/maven"
	);

	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->addContent(new QtProjectWizzardContentPathSourceMaven(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentPathDependenciesMaven(m_settings, window));

			window->setup();
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(advancedSettingsJava()));
	window->updateSubTitle("Indexed Paths");
}

void QtProjectWizzard::advancedSettingsCxx()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->addContent(new QtProjectWizzardContentFlags(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentPathsExclude(m_settings, window));

			window->setup();
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(showSummary()));
	window->updateSubTitle("Advanced (optional)");
}

void QtProjectWizzard::advancedSettingsJava()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->addContent(new QtProjectWizzardContentExtensions(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentPathsExclude(m_settings, window));

			window->setup();
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(showSummaryJava()));
	window->updateSubTitle("Advanced (optional)");
}

void QtProjectWizzard::showSummary()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->setIsForm(true);

			QtProjectWizzardContentBuildFile* buildFile = new QtProjectWizzardContentBuildFile(m_settings, window);
			const bool isCDB = buildFile->getType() == PROJECT_CXX_CDB || buildFile->getType() == PROJECT_CXX_VS;

			if (!isCDB)
			{
				summary->addContent(new QtProjectWizzardContentData(m_settings, window, m_editing));
				summary->addSpace();

				summary->addContent(new QtProjectWizzardContentPathsSource(m_settings, window));
				summary->addContent(new QtProjectWizzardContentExtensions(m_settings, window));
				summary->addSpace();
			}
			else
			{
				summary->addContent(new QtProjectWizzardContentDataCDB(m_settings, window, m_editing));
				summary->addSpace();

				summary->addContent(new QtProjectWizzardContentPathsCDBHeader(m_settings, window));
				summary->addSpace();
			}

			summary->addContent(new QtProjectWizzardContentPathsHeaderSearch(m_settings, window, isCDB));

			std::shared_ptr<CxxProjectSettings> cxxSettings = std::dynamic_pointer_cast<CxxProjectSettings>(m_settings);
			if (!isCDB && cxxSettings && cxxSettings->getHasDefinedUseSourcePathsForHeaderSearch())
			{
				summary->addSpace();
				summary->addContent(new QtProjectWizzardContentSimple(m_settings, window));
			}

			summary->addContent(new QtProjectWizzardContentPathsHeaderSearchGlobal(m_settings, window));
			summary->addSpace();

			if (QSysInfo::macVersion() != QSysInfo::MV_None)
			{
				summary->addContent(new QtProjectWizzardContentPathsFrameworkSearch(m_settings, window, isCDB));
				summary->addContent(new QtProjectWizzardContentPathsFrameworkSearchGlobal(m_settings, window));
				summary->addSpace();
			}

			summary->addContent(new QtProjectWizzardContentFlags(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentPathsExclude(m_settings, window));

			window->setup();

			if (m_editing)
			{
				window->updateTitle("EDIT PROJECT");
				window->updateNextButton("Save");
				window->setPreviousVisible(false);
			}
			else
			{
				window->updateSubTitle("Summary");
				window->updateNextButton("Create");
			}
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(createProject()));
}

void QtProjectWizzard::showSummaryJava()
{
	QtProjectWizzardWindow* window = createWindowWithSummary(
		[this](QtProjectWizzardWindow* window, QtProjectWizzardContentSummary* summary)
		{
			summary->setIsForm(true);

			bool isMaven = false;
			std::shared_ptr<JavaProjectSettings> javaSettings = std::dynamic_pointer_cast<JavaProjectSettings>(m_settings);
			if (javaSettings)
			{
				isMaven = javaSettings->getAbsoluteMavenProjectFilePath().exists();
			}


			summary->addContent(new QtProjectWizzardContentData(m_settings, window, m_editing));
			summary->addSpace();

			if (isMaven)
			{
				summary->addContent(new QtProjectWizzardContentPathSourceMaven(m_settings, window));
				summary->addSpace();
				summary->addContent(new QtProjectWizzardContentPathDependenciesMaven(m_settings, window));
			}
			else
			{
				summary->addContent(new QtProjectWizzardContentPathsSource(m_settings, window));
				summary->addSpace();
				summary->addContent(new QtProjectWizzardContentPathsClassJava(m_settings, window));
			}

			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentExtensions(m_settings, window));
			summary->addSpace();
			summary->addContent(new QtProjectWizzardContentPathsExclude(m_settings, window));

			window->setup();

			if (m_editing)
			{
				window->updateTitle("EDIT PROJECT");
				window->updateNextButton("Save");
				window->setPreviousVisible(false);
			}
			else
			{
				window->updateSubTitle("Summary");
				window->updateNextButton("Create");
			}
		}
	);

	connect(window, SIGNAL(next()), this, SLOT(createProject()));
}

void QtProjectWizzard::createProject()
{
	FilePath path = m_settings->getFilePath();

	m_settings->setVersion(ProjectSettings::VERSION);
	m_settings->save(path);

	bool forceRefreshProject = false;
	if (m_editing)
	{
		bool settingsChanged = false;

		Application* application = Application::getInstance().get();
		if (application->getCurrentProject() != NULL)
		{
			settingsChanged = !(application->getCurrentProject()->settingsEqualExceptNameAndLocation(*(m_settings.get())));
		}

		bool appSettingsChanged = !(m_appSettings == *ApplicationSettings::getInstance().get());

		if (settingsChanged || appSettingsChanged)
		{
			forceRefreshProject = true;
		}
	}
	else
	{
		MessageStatus("Created project: " + path.str()).dispatch();
	}

	MessageLoadProject(path, forceRefreshProject).dispatch();

	finishWizzard();
}

void QtProjectWizzard::savePreferences()
{
	bool appSettingsChanged = !(m_appSettings == *ApplicationSettings::getInstance().get());

	if (m_appSettings.getScrollSpeed() != ApplicationSettings::getInstance()->getScrollSpeed())
	{
		MessageScrollSpeedChange(ApplicationSettings::getInstance()->getScrollSpeed()).dispatch();
	}

	if (m_appSettings.getCoatiPort() != ApplicationSettings::getInstance()->getCoatiPort() ||
		m_appSettings.getPluginPort() != ApplicationSettings::getInstance()->getPluginPort())
	{
		MessagePluginPortChange().dispatch();
	}

	Application::getInstance()->loadSettings();

	if (appSettingsChanged)
	{
		Project* currentProject = Application::getInstance()->getCurrentProject().get();
		if (currentProject)
		{
			MessageLoadProject(currentProject->getProjectSettingsFilePath(), true).dispatch();
		}
	}
	else
	{
		MessageRefresh().refreshUiOnly().dispatch();
	}

	cancelWizzard();
}
