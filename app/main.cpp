#include "AppContext.h"
#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    // Qt Widgets 程序入口：负责初始化 Qt 应用对象。
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Workflow");
    QApplication::setApplicationName("Visual Workflow Studio");

    // AppContext 是整个程序的依赖装配中心。
    // 服务、执行器、Worker 注册表等长期对象都在这里创建，避免散落在 UI 代码里。
    vws::AppContext appContext;
    appContext.initialize();

    // MainWindow 只负责界面拼装；真正的业务能力通过 AppContext 暴露。
    vws::MainWindow mainWindow(appContext);
    mainWindow.resize(1440, 900);
    mainWindow.show();

    return QApplication::exec();
}
