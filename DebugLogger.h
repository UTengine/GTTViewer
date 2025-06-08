#pragma once

class DebugLogger {
public:
    static void log(const QString& message) {
#ifdef _DEBUG
        qDebug() << message;
#else
        static QFile logFile("LogFile.txt");
        if (!logFile.isOpen()) {
            logFile.open(QIODevice::Append | QIODevice::Text);
        }
        if (logFile.isOpen()) {
            QTextStream out(&logFile);
            out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " - "
                << message << "\n";
            out.flush();
        }
#endif
    }
};