#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QStackedWidget>

class QQuickWidget;
class MainUIBackend;
class MdiView;
class LogosAPI;

class MainContainer : public QWidget
{
    Q_OBJECT

public:
    explicit MainContainer(LogosAPI* logosAPI = nullptr, QWidget* parent = nullptr);
    ~MainContainer();

    // Get the MDI view
    MdiView* getMdiView() const { return m_mdiView; }

    // Get the backend
    MainUIBackend* getBackend() const { return m_backend; }

    // Get the LogosAPI instance
    LogosAPI* getLogosAPI() const { return m_logosAPI; }

    // Get the detached sidebar window (nullptr if sidebar is embedded)
    QWidget* getSidebarWindow() const { return m_sidebarWindow; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onViewIndexChanged();
    void onNavigateToApps();
    void onPluginWindowRequested(QWidget* widget, const QString& title);
    void onPluginWindowRemoveRequested(QWidget* widget);
    void onPluginWindowActivateRequested(QWidget* widget);
    void onOverlayActiveChanged(bool active);
    // Sync detached sidebar visibility with main window
    void onMainWindowVisibilityChanged();

private:
    void setupUi();
    QUrl resolveQmlUrl(const QString& qmlFile);

    // Sidebar creation - two modes
    void createEmbeddedSidebar();
    void createDetachedSidebar();

    // Helper: configure a QQuickWidget for sidebar QML (shared by both modes)
    QQuickWidget* createSidebarWidget(QWidget* parent);

    // Connect sidebar QML signals to backend - works for both embedded and detached
    void connectSidebarSignals();

    // Main layout
    QHBoxLayout* m_mainLayout;

    // Sidebar (QML) - embedded mode only
    QQuickWidget* m_sidebarWidget;

    // Detached sidebar window - detached mode only
    QWidget* m_sidebarWindow;

    // Content area
    QStackedWidget* m_contentStack;

    // MdiView (C++ widget for Apps)
    MdiView* m_mdiView;

    // Content views (QML for Dashboard, Modules, PackageManager, Settings)
    QQuickWidget* m_contentWidget;

    // Full-window overlay for dependency-aware dialogs
    QQuickWidget* m_overlayWidget;

    // Backend
    MainUIBackend* m_backend;

    // LogosAPI instance
    LogosAPI* m_logosAPI;
};
