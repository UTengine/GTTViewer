#include "stdafx.h"
#include "MainWindow.h"
#include "ui_mainwindow.h"
#include "D3DRenderWidget.h"

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    d3dWidget = findChild<D3DRenderWidget*>("renderWidget");
    QVBoxLayout* layout = new QVBoxLayout(ui->centralwidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d3dWidget);
    d3dWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(ui->actionOpen_GTT, &QAction::triggered, this, &MainWindow::onOpenGTT);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onAbout);
    statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(statusLabel);
    statusLabel->setText("Ready");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onAbout()
{
    QMessageBox box;
    box.setWindowTitle("About");
    box.setText("Sie amk");
    QIcon pimpmap(loadAppIcon(96));
    QPixmap pixmap = pimpmap.pixmap(64, 64);
    box.setIconPixmap(pixmap);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void MainWindow::onOpenGTT()
{
    QString initialDir = readLastDirectory();
    QString fileName = QFileDialog::getOpenFileName(this, "Open GTT File", initialDir, "GTT Files (*.GTT *.DXT)");
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Could not open file.");
        return;
    }
    QByteArray rawData;
    QVector<qint64> textureOffsets;
    qint64 dataSize;
    rawData = file.readAll();
    file.close();
    textureOffsets.clear();

    const char* raw = rawData.constData();
    dataSize = rawData.size();
    QMessageBox::information(this, "GTT or DXT Loaded", QString("Loaded %1 bytes").arg(rawData.size()));
    int countNTF3 = 0;
    int countNTF7 = 0;

    for (qint64 i = 0; i <= dataSize - 4; ++i) {
        if (raw[i] == 'N' && raw[i + 1] == 'T' && raw[i + 2] == 'F') {
            unsigned char version = static_cast<unsigned char>(raw[i + 3]);
            if (version == 0x03 || version == 0x07) {
                textureOffsets.append(i);
                if (version == 0x03) countNTF3++;
                else if (version == 0x07) countNTF7++;
            }
        }
    }
    if (textureOffsets.isEmpty()) 
    {
        QMessageBox::warning(this, "No Textures Found", "No NTF headers found in file.");
        return;
    }
    else 
    {
        QString msg = "Found ";
        if (countNTF3 > 0) msg += QString("%1 NTF3").arg(countNTF3);
        if (countNTF3 > 0 && countNTF7 > 0) msg += " and ";
        if (countNTF7 > 0) msg += QString("%1 NTF7").arg(countNTF7);
        msg += " textures.";
        QMessageBox::information(this, "Headers Found", msg);
    }
#ifdef _DEBUG
    qDebug() << "Loading" << textureOffsets.size() << "textures";
#else
    DebugLogger::log(QString("FileName: %1").arg(fileName));
    DebugLogger::log(QString("Loading %1 textures").arg(textureOffsets.size()));
#endif

    QString fileInfo = QFileInfo(fileName).fileName();
    QString folder = QFileInfo(fileName).dir().dirName();
    statusLabel->setText(QString("Opened: %1 (%2 textures)").arg(folder + "\\" + fileInfo).arg(textureOffsets.size()));

    saveLastDirectory(QFileInfo(fileName).absolutePath());

    QString baseName = QFileInfo(fileName).completeBaseName();
    d3dWidget->loadTextures(rawData, textureOffsets, baseName);
}

QPixmap MainWindow::loadAppIcon(int size)
{
    HICON hIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101),  
            IMAGE_ICON,size,size,LR_DEFAULTCOLOR)); // 101 = IDI_APP_ICON
    if (!hIcon)
        return QPixmap();
    QImage image = QImage::fromHICON(hIcon);
    DestroyIcon(hIcon);
    return QPixmap::fromImage(image);
}

QString MainWindow::readLastDirectory()
{
    QFile file("settings.txt");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) 
    {
        QTextStream in(&file);
        while (!in.atEnd()) 
        {
            QString line = in.readLine();
            if (line.startsWith("last_directory=")) 
            {
                return line.section('=', 1);
            }
        }
    }
    return ""; // fallback
}

void MainWindow::saveLastDirectory(const QString& path)
{
    QFile file("settings.txt");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) 
    {
        QTextStream out(&file);
        out << "last_directory=" << path << "\n";
    }
}
