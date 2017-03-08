#ifndef QT_PROJECT_WIZZARD_H
#define QT_PROJECT_WIZZARD_H

#include <QWidget>

#include "qt/window/project_wizzard/QtProjectWizzardContentSelect.h"
#include "qt/window/QtWindowStack.h"

#include "settings/ApplicationSettings.h"
#include "settings/ProjectSettings.h"

#include "utility/solution/SolutionParserManager.h"

class QtProjectWizzardContentSummary;
class QtProjectWizzardWindow;

class QtProjectWizzard
	: public QtWindowStackElement
{
	Q_OBJECT

signals:
	void finished();
	void canceled();

public:
	QtProjectWizzard(QWidget* parent = nullptr);

	// QtWindowStackElement implementation
	virtual void showWindow() override;
	virtual void hideWindow() override;

public slots:
	void newProject();

	void newProjectFromSolution(const std::string& ideId, const std::string& solutionPath);
	void newProjectFromCDB(const std::string& filePath, const std::vector<std::string>& headerPaths);
	void refreshProjectFromSolution(const std::string& ideId, const std::string& solutionPath);

	void editProject(const FilePath& settingsPath);
	void editProject(std::shared_ptr<ProjectSettings> settings);
	void showPreferences();

private:
	static bool applicationSettingsContainVisualStudioHeaderSearchPaths();

	template<typename T>
		QtProjectWizzardWindow* createWindowWithContent();

	QtProjectWizzardWindow* createWindowWithSummary(
		std::function<void(QtProjectWizzardWindow*, QtProjectWizzardContentSummary*)> func);

	QtWindowStack m_windowStack;

	std::shared_ptr<ProjectSettings> m_settings;
	ApplicationSettings m_appSettings;

	bool m_editing;

	std::shared_ptr<SolutionParserManager> m_parserManager;

private slots:
	void cancelWizzard();
	void finishWizzard();

	void windowStackChanged();

	void selectedProjectType(ProjectType type);

	void emptyProject();

	void sourcePaths();
	void headerSearchPaths();

	void headerSearchPathsDone();
	void frameworkSearchPaths();

	void emptyProjectCDBVS();
	void emptyProjectCDB();
	void emptyProjectJavaMaven();

	void headerPathsCDB();

	void sourcePathsJava();
	void sourcePathsJavaMaven();

	void advancedSettingsCxx();
	void advancedSettingsJava();

	void showSummary();
	void showSummaryJava();

	void createProject();
	void savePreferences();
};

template<>
QtProjectWizzardWindow* QtProjectWizzard::createWindowWithContent<QtProjectWizzardContentSelect>();

#endif // QT_PROJECT_WIZZARD_H
