#define NOMINMAX 
#include <QtWidgets>
#include <QMetaObject>
#include <algorithm>
#include <QtCore/qglobal.h>
#include <QByteArray>
#include <QVector>
#include <QWidget>
#include <QFile>
#include <QResizeEvent>
#include <QDebug>
#include <vector>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <windows.h> // for wincrypt
#include <wincrypt.h>
#include <cmath>
#include <cassert>
#include "CWinCrypt.h"
#if QT_VERSION >= 0x050000
#include <QtWidgets/QMainWindow>
#else
#include <QtGui/QMainWindow>
#endif
#if QT_VERSION >= 0x050000
#include <QtWidgets/QApplication>
#else
#include <QtGui/QApplication>
#endif
#include "DebugLogger.h"