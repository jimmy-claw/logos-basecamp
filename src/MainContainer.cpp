#include "MainContainer.h"
#include "MainUIBackend.h"
#include "mdiview.h"
#include "SkinConfig.h"

#include <QQuickWidget>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QVBoxLayout>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QQuickItem>
#include <QProcessEnvironment>
#include <QColor>
#include <QPalette>
#include <QTimer>

MainContainer::MainContainer(LogosAPI* logosAPI, QWidget* parent)
    : QWidget(parent)
    , m_logosAPI(logosAPI)
    , m_backend(nullptr)
    , m_sidebarWidget(nullptr)
    , m_sidebarWindow(nullptr)
    , m_contentStack(nullptr)
    , m_mdiView(nullptr)
    , m_contentWidget(nullptr)
    , m_overlayWidget(nullptr)
{
    QQuickStyle::setStyle("Basic");

    m_backend = new MainUIBackend(m_logosAPI, this);

    setupUi();

    // Connect section index changes
    connect(m_backend, &MainUIBackend::currentActiveSectionIndexChanged,
            this, &MainContainer::onViewIndexChanged);
    connect(m_backend, &MainUIBackend::navigateToApps,
            this, &MainContainer::onNavigateToApps);

    // Connect plugin window signals to MdiView
    connect(m_backend, &MainUIBackend::pluginWindowRequested,
            this, &MainContainer::onPluginWindowRequested);
    connect(m_backend, &MainUIBackend::pluginWindowRemoveRequested,
            this, &MainContainer::onPluginWindowRemoveRequested);
    connect(m_backend, &MainUIBackend::pluginWindowActivateRequested,
            this, &MainContainer::onPluginWindowActivateRequested);

    // When user closes a plugin window (tab/window X), notify backend to unload
    connect(m_mdiView, &MdiView::pluginWindowClosed,
            m_backend, &MainUIBackend::onPluginWindowClosed);

    // Connect sidebar QML signals to backend (works for both embedded and detached)
    connectSidebarSignals();

    qDebug() << "MainContainer created";
}

MainContainer::~MainContainer()
{
    // Detached sidebar window has no parent - must delete explicitly
    if (m_sidebarWindow) {
        m_sidebarWindow->deleteLater();
        m_sidebarWindow = nullptr;
    }
    qDebug() << "MainContainer destroyed";
}

QUrl MainContainer::resolveQmlUrl(const QString& qmlFile)
{
    QString qmlUiPath = QProcessEnvironment::systemEnvironment().value("QML_UI", "");

    if (!qmlUiPath.isEmpty()) {
        QDir qmlDir(qmlUiPath);
        QString fullPath = qmlDir.absoluteFilePath(qmlFile);

        if (QFile::exists(fullPath)) {
            qDebug() << "Loading from filesystem" << fullPath;
            return QUrl::fromLocalFile(fullPath);
        }
    }

    qDebug() << "Loading from resources" << qmlFile;
    return QUrl("qrc:/" + qmlFile);
}

// --- Sidebar creation (shared helper) ---

QQuickWidget* MainContainer::createSidebarWidget(QWidget* parent)
{
    QString qmlUiPath = QProcessEnvironment::systemEnvironment().value("QML_UI", "");

    QQuickWidget* sidebar = new QQuickWidget(parent);
    sidebar->setResizeMode(QQuickWidget::SizeRootObjectToView);

    if (!qmlUiPath.isEmpty()) {
        QString absPath = QDir(qmlUiPath).absolutePath();
        sidebar->engine()->addImportPath(absPath + "/qml");
        sidebar->engine()->addImportPath(absPath);
        qDebug() << "DEV MODE: Added QML import paths:" << absPath + "/qml" << absPath;
    } else {
        sidebar->engine()->addImportPath("qrc:/qml");
    }
    qDebug() << "Sidebar engine import paths:" << sidebar->engine()->importPathList();

    sidebar->rootContext()->setContextProperty("backend", m_backend);
    sidebar->setSource(resolveQmlUrl("qml/panels/SidebarPanel.qml"));
    sidebar->setMinimumWidth(60);
    sidebar->setMaximumWidth(60);

    // Apply theme color from skin config (or default)
    QString bgColorStr = SkinConfig::instance()
                           ? SkinConfig::instance()->themeBackground()
                           : "#171717";
    sidebar->setClearColor(QColor(bgColorStr));

    return sidebar;
}

void MainContainer::createEmbeddedSidebar()
{
    m_sidebarWidget = createSidebarWidget(this);
}

void MainContainer::createDetachedSidebar()
{
    qDebug() << "[Skin] Creating detached sidebar window";

    // Create a standalone QWidget window for the sidebar
    m_sidebarWindow = new QWidget(nullptr);  // no parent = top-level window
    m_sidebarWindow->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    m_sidebarWindow->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    m_sidebarWindow->setWindowTitle("Logos Sidebar");

    QString bgColorStr = SkinConfig::instance()
                           ? SkinConfig::instance()->themeBackground()
                           : "#171717";
    QColor bgColor(bgColorStr);
    m_sidebarWindow->setAutoFillBackground(true);
    QPalette p = m_sidebarWindow->palette();
    p.setColor(QPalette::Window, bgColor);
    m_sidebarWindow->setPalette(p);

    // Layout MUST be created before adding child widgets so they are
    // automatically managed by the layout (Qt does not retroactively
    // add existing children to a new layout).
    QVBoxLayout* layout = new QVBoxLayout(m_sidebarWindow);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);

    // Create sidebar widget inside the window (auto-added to layout).
    // Store pointer for signal connections (avoids fragile child search).
    m_sidebarWidget = createSidebarWidget(m_sidebarWindow);
    // The sidebar QQuickWidget is auto-added to layout because it has m_sidebarWindow as parent

    m_sidebarWindow->setMinimumWidth(60);
    m_sidebarWindow->setMaximumWidth(60);
    m_sidebarWindow->setMinimumHeight(400);

    // Position will be set after main window is shown (we need its geometry)
    m_sidebarWindow->show();

    int x = SkinConfig::instance() ? SkinConfig::instance()->sidebarDefaultX() : -76;
    int y = SkinConfig::instance() ? SkinConfig::instance()->sidebarDefaultY() : 200;

    QTimer::singleShot(100, this, [this, x, y]() {
        if (!m_sidebarWindow || !m_sidebarWindow->isVisible()) return;
        QWidget* mainWindow = window();
        if (mainWindow && mainWindow->isVisible()) {
            m_sidebarWindow->move(
                mainWindow->geometry().left() + x,
                mainWindow->geometry().top() + y
            );
        } else {
            m_sidebarWindow->move(x, y);
        }
        qDebug() << "[Skin] Positioned detached sidebar at" << m_sidebarWindow->pos();
    });

    // Sync visibility: when main window hides (minimize-to-tray), hide sidebar too
    connect(window(), &QWidget::visibilityChanged,
            this, &MainContainer::onMainWindowVisibilityChanged);
}

void MainContainer::onMainWindowVisibilityChanged(bool visible)
{
    if (m_sidebarWindow) {
        m_sidebarWindow->setVisible(visible);
        if (visible) m_sidebarWindow->raise();
    }
}

// Connect sidebar QML signals to backend - works for both embedded and detached modes.
void MainContainer::connectSidebarSignals()
{
    // m_sidebarWidget is set in both embedded and detached modes now
    if (!m_sidebarWidget) {
        qWarning() << "[MainContainer] No sidebar widget found for signal connections";
        return;
    }

    QObject* sidebarRoot = m_sidebarWidget->rootObject();
    if (!sidebarRoot) {
        qWarning() << "[MainContainer] Sidebar root object is null (QML not loaded yet?)";
        return;
    }

    // launchUIModule uses QueuedConnection - the signal is emitted from a
    // SidebarAppDelegate.onClicked handler inside a Repeater delegate.
    // onAppLauncherClicked calls setCurrentVisibleApp which synchronously
    // emits launcherAppsChanged, causing both sidebar Repeaters to reset
    // their models. If the connection were direct the Repeater would call
    // setParentItem(nullptr) on the clicked delegate while its click handler
    // is still on the call stack, leading to a null deref in
    // QQuickItemPrivate::derefWindow. Queuing the call lets the click handler
    // return before any Repeater model update fires.
    connect(sidebarRoot, SIGNAL(launchUIModule(QString)),
            m_backend, SLOT(onAppLauncherClicked(QString)),
            Qt::QueuedConnection);
    connect(sidebarRoot, SIGNAL(updateLauncherIndex(int)),
            m_backend, SLOT(setCurrentActiveSectionIndex(int)));

    qDebug() << "[MainContainer] Sidebar signals connected";
}

// --- Main UI setup ---

void MainContainer::setupUi()
{
    // Apply theme background from skin config (or default)
    QString bgColorStr = SkinConfig::instance()
                           ? SkinConfig::instance()->themeBackground()
                           : "#171717";
    QColor bgColor(bgColorStr);

    if (SkinConfig::instance()) {
        qDebug() << "[Skin] Applying skin:" << SkinConfig::instance()->skinName()
                 << "background:" << bgColorStr;
    }

    setAutoFillBackground(true);
    QPalette p = palette();
    p.setColor(QPalette::Window, bgColor);
    setPalette(p);

    // Create main horizontal layout
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(4, 0, 4, 2);

    // === SIDEBAR - decide mode ===
    bool detached = SkinConfig::instance() && SkinConfig::instance()->isSidebarDetached();

    if (detached) {
        createDetachedSidebar();
        // No sidebar in the main layout - it floats separately
    } else {
        createEmbeddedSidebar();
        m_mainLayout->addWidget(m_sidebarWidget);
    }

    // === CONTENT AREA (vertical layout with stack + app launcher) ===
    QWidget* contentArea = new QWidget(this);
    QVBoxLayout* contentLayout = new QVBoxLayout(contentArea);
    contentLayout->setSpacing(0);
    contentLayout->setContentsMargins(4, 9, 4, 4);

    // Create content stack
    m_contentStack = new QStackedWidget(contentArea);
    m_contentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Index 0: MdiView (C++ widget)
    m_mdiView = new MdiView(m_contentStack);
    m_contentStack->addWidget(m_mdiView);

    // Index 1: QML content views (Dashboard, Modules, PackageManager, Settings)
    QString qmlUiPath = QProcessEnvironment::systemEnvironment().value("QML_UI", "");
    m_contentWidget = new QQuickWidget(m_contentStack);
    m_contentWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    if (!qmlUiPath.isEmpty()) {
        QString absPath = QDir(qmlUiPath).absolutePath();
        m_contentWidget->engine()->addImportPath(absPath + "/qml");
        m_contentWidget->engine()->addImportPath(absPath);
    } else {
        m_contentWidget->engine()->addImportPath("qrc:/qml");
    }
    m_contentWidget->rootContext()->setContextProperty("backend", m_backend);
    m_contentWidget->setSource(resolveQmlUrl("qml/views/ContentViews.qml"));
    m_contentStack->addWidget(m_contentWidget);

    contentLayout->addWidget(m_contentStack, 1);
    m_mainLayout->addWidget(contentArea, 1);

    // === OVERLAY DIALOGS (QML) ===
    m_overlayWidget = new QQuickWidget(this);
    m_overlayWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_overlayWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_overlayWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_overlayWidget->setClearColor(Qt::transparent);
    m_overlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    if (!qmlUiPath.isEmpty()) {
        QString absPath = QDir(qmlUiPath).absolutePath();
        m_overlayWidget->engine()->addImportPath(absPath + "/qml");
        m_overlayWidget->engine()->addImportPath(absPath);
    } else {
        m_overlayWidget->engine()->addImportPath("qrc:/qml");
    }
    m_overlayWidget->rootContext()->setContextProperty("backend", m_backend);
    m_overlayWidget->setSource(resolveQmlUrl("qml/views/OverlayDialogs.qml"));

    if (QObject* overlayRoot = m_overlayWidget->rootObject()) {
        connect(overlayRoot, SIGNAL(overlayActiveChanged(bool)),
                this, SLOT(onOverlayActiveChanged(bool)));
    }

    m_contentStack->setCurrentIndex(0);
    setMinimumSize(800, 600);
}

void MainContainer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_overlayWidget) {
        m_overlayWidget->setGeometry(0, 0, width(), height());
        m_overlayWidget->raise();
    }
}

void MainContainer::onOverlayActiveChanged(bool active)
{
    if (!m_overlayWidget) return;
    m_overlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents, !active);
    if (active) m_overlayWidget->raise();
}

void MainContainer::onViewIndexChanged()
{
    int sectionIndex = m_backend->currentActiveSectionIndex();
    qDebug() << "MainContainer: Active section index changed to" << sectionIndex;

    if (sectionIndex == 0) {
        m_contentStack->setCurrentIndex(0);
    } else {
        m_contentStack->setCurrentIndex(1);
    }
}

void MainContainer::onNavigateToApps()
{
    m_backend->setCurrentActiveSectionIndex(0);
}

void MainContainer::onPluginWindowRequested(QWidget* widget, const QString& title)
{
    if (m_mdiView && widget) {
        m_mdiView->addPluginWindow(widget, title);
        qDebug() << "MainContainer: Added plugin window to MdiView:" << title;
    }
}

void MainContainer::onPluginWindowRemoveRequested(QWidget* widget)
{
    if (m_mdiView && widget) {
        m_mdiView->removePluginWindow(widget);
        qDebug() << "MainContainer: Removed plugin window from MdiView";
    }
}

void MainContainer::onPluginWindowActivateRequested(QWidget* widget)
{
    if (m_mdiView && widget) {
        m_mdiView->activatePluginWindow(widget);
        qDebug() << "MainContainer: Activated plugin window in MdiView";
    }
}
