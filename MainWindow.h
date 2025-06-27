#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "GTTLoader.h"
#include "D3DRenderWidget.h"


class D3DRenderWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    QPixmap loadAppIcon(int size = 96);
    QString readLastDirectory();
    void saveLastDirectory(const QString& path);
private slots:
    void onOpenGTT();
    void onAbout();
private:
    QLabel* statusLabel;
    Ui::MainWindow* ui;
    D3DRenderWidget* d3dWidget = nullptr;
};
#endif