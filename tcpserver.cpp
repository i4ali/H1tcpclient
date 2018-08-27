#include <QtEndian>
#include <QJsonDocument>
#include <sys/utsname.h>
#include <arpa/inet.h>
#include <string>

#include "tcpserver.h"
#include "mainwindow.h"
#include "liveviewscreen.h"
#include "imageviewlist.h"
#include "systeminterface.h"
#include "systemfunctions.h"
#include "revision.h"

TcpServer::TcpServer(QObject *parent,int port) : QObject(parent)
{
    tcpServer = new QTcpServer(this);
    // whenever a user connects, it will emit signal
    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(tcpNewConnection()));

    if(!tcpServer->listen(QHostAddress::Any, port))
    {
        qDebug() << "tcpServer could not start";
    }
    else
    {
        qDebug() << "tcpServer started!";
    }
}

void TcpServer::tcpNewConnection()
{
    QTcpSocket *tcpSocket = tcpServer->nextPendingConnection();
    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(tcpReadyRead()), Qt::DirectConnection);
    connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(tcpDisconnected()));

    qDebug() << "New connection from " << tcpSocket->peerAddress() << ":" << tcpSocket->peerPort();
    tcpBuffers.insert(tcpSocket,new QByteArray());
}

void TcpServer::tcpDisconnected()
{
    QTcpSocket *tcpSocket = static_cast<QTcpSocket*>(sender());
    QByteArray *buffer = tcpBuffers.value(tcpSocket);
    qDebug() << "Disconnection from " << tcpSocket->peerAddress() << ":" << tcpSocket->peerPort();
    tcpBuffers.remove(tcpSocket);
    delete buffer;
    tcpSocket->deleteLater();
}

void TcpServer::tcpReadyRead()
{
    QTcpSocket *tcpSocket = static_cast<QTcpSocket*>(sender());
    QByteArray *buffer = tcpBuffers.value(tcpSocket);
    if (!buffer)
    {
        qDebug() << "Connection not found";
    }
    else
    {
        QByteArray tmp = tcpSocket->readAll();
        *buffer += tmp;
        for(;;)
        {
            if (buffer->size() < 4)
            {
                break;
            }
            uint32_t length = qFromBigEndian<qint32>((uchar *)buffer->data());
            if (buffer->size() < length)
            {
                break;
            }

            QByteArray message = *buffer;
            message.remove(0,4);
            message.truncate(length-4);

            processTcpMessage(tcpSocket,message);
            buffer->remove(0, length);
        }
    }
}

int TcpServer::sendMessage(QTcpSocket *tcpSocket,const QByteArray &message,TCPMessageType t,bool more)
{
    QByteArray l(8,'\0');
    qToBigEndian<qint32>(message.size()+8,(uchar *)l.data());
    ((uchar *)l.data())[4] = t;
    ((uchar *)l.data())[5] = more ? 1 : 0;
    return tcpSocket->write(l + message);
}

QJsonObject CameraStatus(int i)
{
    QJsonObject camera;
    camera["id"] = i;
    camera["recording"] = systemFunctions.IsRecording(i);
    camera["postrecordingend"] = (int)systemFunctions.PostRecordingEnd(i);
    camera["recordingfailsafe"] = SystemFunctions::IsRecordingFailsafe(i);
    camera["resolution"] = MainWindow::GlobalVO->SC_cam_Resolution[i];
    return camera;
}

void TcpServer::processTcpMessage(QTcpSocket *tcpSocket,QByteArray &message)
{
    if (message.size() < 4)
    {
         qDebug() << "message too short for header";
    }
    else if (message[0] == (char)tmt_JSON)
    {
        message.remove(0,4);
        processJsonMessage(tcpSocket,message);
    }
    else
    {
        qDebug() << "unhandled message type " << (int)message[0];
    }
}

void TcpServer::handle_ping(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_SUCCESS;
    sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"ping\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
}

void TcpServer::handle_readfile(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    if (! cmdobject["filename"].isString())
    {
        qDebug() << "No file name";
        sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"readfile\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
    }
    else
    {
        QString name = cmdobject["filename"].toString();
        if (name.size() > 0 && name[0] != '/')
        {
            name = MainWindow::GlobalVO->XML_PATH + name;
        }
        QFile file(name);

        if (file.open(QIODevice::ReadOnly))
        {
            rc = STS_SUCCESS;
            sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"readfile\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));

            int bytes_to_write;
            while(!file.atEnd())
            {
                char data[4096];
                bytes_to_write = file.read(data,4096);
                sendMessage(tcpSocket,QByteArray(data,bytes_to_write),tmt_BINARY,true);
            }
            sendMessage(tcpSocket,QByteArray(),tmt_BINARY,false);
            file.close();
        }
        else
        {
            qDebug() << "Could not open requested:" << name;
            sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"readfile\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
        }
    }
}

void TcpServer::handle_cm_starttransfer(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->StartTransfer();

    r["command"] = "cm_starttransfer";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_cm_stoptransfer(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->StopTransfer();

    r["command"] = "cm_stoptransfer";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_cm_startx1import(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->StartX1Import();

    r["command"] = "cm_startx1import";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_cm_stopx1import(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->StopX1Import();

    r["command"] = "cm_stopx1import";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_cm_remakeconnection(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->RemakeConnection();

    r["command"] = "cm_remakeconnection";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_wmicenable(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->EnableWMICs();

    r["command"] = "mm_wmicenable";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_wmicdisable(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->DisableWMICs();

    r["command"] = "mm_wmicdisable";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_wmiccoverton(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->WMICCovertOn();

    r["command"] = "mm_wmiccoverton";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_wmiccovertoff(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->WMICCovertOff();

    r["command"] = "mm_wmiccovertoff";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_covertinterviewoff(QTcpSocket *tcpSocket,QJsonObject &)
{
    QJsonObject r;

    systemFunctions.setCovertInterviewMode(false);

    r["command"] = "mm_covertinterviewoff";
    r["status"] = STS_SUCCESS;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_covertinterviewon(QTcpSocket *tcpSocket,QJsonObject &)
{
    QJsonObject r;

    systemFunctions.setCovertInterviewMode(true);

    r["command"] = "mm_covertinterviewon";
    r["status"] = STS_SUCCESS;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_wmicon(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;
    rc = systemInterface->WMICOn();

    r["command"] = "mm_wmicon";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_wmicoff(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;
    rc = systemInterface->WMICOff();

    r["command"] = "mm_wmicoff";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_speakermuteon(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->SpeakerMuteOn();

    r["command"] = "mm_speakermuteon";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_mm_speakermuteoff(QTcpSocket *tcpSocket,QJsonObject &)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->SpeakerMuteOff();

    r["command"] = "mm_speakermuteoff";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_streamfileduration(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;
    int32_t file_duration;
    if (! cmdobject["filename"].isString())
    {
        qDebug() << "No file name";
    }
    else
    {
        rc = systemInterface->StreamFileDuration(cmdobject["filename"].toString(), file_duration);
    }

    r["command"] = "pm_streamfileduration";
    r["status"] = rc;
    r["duration"] = file_duration;

    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());

    /* Need to close the file handle */
    rc = systemInterface->PlayCloseFile();
}

void TcpServer::handle_pm_streamstartfile(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;
    if (! cmdobject["filename"].isString())
    {
        qDebug() << "No file name";
    }
    else
    {
        rc = systemInterface->StreamStartFile(cmdobject["filename"].toString());
    }
    r["command"] = "pm_streamstartfile";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_setosdcontent(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    int x = 0;
    int y = 0;

    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else if (! cmdobject["content"].isString())
    {
        qDebug() << "no content";
    }
    else if (! cmdobject["block"].isDouble())
    {
        qDebug() << "no block";
    }
    else
    {
        if (cmdobject["x"].isDouble()) x = cmdobject["x"].toInt();
        if (cmdobject["y"].isDouble()) x = cmdobject["y"].toInt();
        rc = systemInterface->SetOSDContent(x,y,cmdobject["camera"].toInt(),cmdobject["block"].toInt(),cmdobject["block"].toString());
    }
    r["command"] = "pm_setosdcontent";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_setosdstats(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    if (! cmdobject["stats"].isDouble())
    {
        qDebug() << "no stats";
    }
    else
    {
        rc = systemInterface->SetOSDStats(cmdobject["stats"].toInt());
    }

    r["command"] = "pm_setosdstats";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_streamstopfile(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    rc = systemInterface->StreamStopFile();

    r["command"] = "pm_streamstopfile";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_fileinfo(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;
    if (! cmdobject["filename"].isString())
    {
        qDebug() << "No file name";
    }
    else
    {
        QString info;
        rc = systemInterface->PlayGetFileInfo(cmdobject["filename"].toString(),info);
        if (rc == STS_SUCCESS)
        {
            r["info"] = info;
        }
    }
    r["command"] = "pm_fileinfo";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_startrecordMP4(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else if (! cmdobject["filename"].isString())
    {
        qDebug() << "no filename";
    }
    else if (! cmdobject["pretime"].isDouble())	// seconds beforenow
    {
        qDebug() << "no pretime";
    }
    else
    {
        rc = systemInterface->StartRecordMP4(cmdobject["filename"].toString(),cmdobject["camera"].toInt(),cmdobject["pretime"].toInt());
    }

    r["command"] = "pm_startrecordmp4";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_stoprecordMP4(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else
    {
        rc = systemInterface->StopRecordMP4(cmdobject["camera"].toInt());
    }

    r["command"] = "pm_stoprecordmp4";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_startrecordTS(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else if (! cmdobject["filename"].isString())
    {
        qDebug() << "no filename";
    }
    else if (! cmdobject["pretime"].isDouble())	// seconds beforenow
    {
        qDebug() << "no pretime";
    }
    else
    {
        rc = systemInterface->StartRecordTS(cmdobject["filename"].toString(),cmdobject["camera"].toInt(),cmdobject["pretime"].toInt());
    }

    r["command"] = "pm_startrecordts";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_stoprecordTS(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else
    {
        rc = systemInterface->StopRecordTS(cmdobject["camera"].toInt());
    }

    r["command"] = "pm_stoprecordts";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_recsyncnextMP4(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else if (! cmdobject["filename"].isString())
    {
        qDebug() << "no filename";
    }
    else
    {
        rc = systemInterface->RecSyncNextMP4(cmdobject["filename"].toString(),cmdobject["camera"].toInt());
    }

    r["command"] = "pm_recsynextmp4";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_recsyncnextTS(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else if (! cmdobject["filename"].isString())
    {
        qDebug() << "no filename";
    }
    else
    {
        rc = systemInterface->RecSyncToNext(cmdobject["filename"].toString(),cmdobject["camera"].toInt());
    }

    r["command"] = "pm_recsynextts";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_record(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else
    {
        int cam_id = cmdobject["camera"].toInt();
        int pre_seconds = -1;
        if (cmdobject["pre"].isDouble())
        {
            pre_seconds = cmdobject["pre"].toInt();
        }
        rc = systemFunctions.StartRecord(cam_id,pre_seconds);
    }
    sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"record\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
}

void TcpServer::handle_stoprecord(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "camera";
    }
    else
    {
        rc = systemFunctions.StopRecord(cmdobject["camera"].toInt());
    }
    sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"stoprecord\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
}

void TcpServer::handle_pm_snapshot(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else if (! cmdobject["filename"].isString())
    {
        qDebug() << "no file";
    }
    else
    {
        rc = systemInterface->Snapshot(cmdobject["camera"].toInt(),cmdobject["filename"].toString());
    }
    sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"pm_snapshot\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
}

void TcpServer::handle_setmic(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    if (! cmdobject["mic"].isString())
    {
    }
    else
    {
        if (! cmdobject["mute"].isBool())
        {
        }
        else
        {
            if (systemFunctions.Mute(cmdobject["mic"].toString(),cmdobject["mute"].toBool()))
            {
                rc = STS_SUCCESS;
            }
        }
    }
    r["command"] = "setmic";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_getmic(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_SUCCESS;
    QJsonObject r;

    bool onlyone = false;
    if (cmdobject["mic"].isString())
    {
        onlyone = true;
    }

    const std::map<QString,bool> &ms = systemFunctions.MicMuteState();
    QJsonArray ca;
    if (onlyone)
    {
        const auto &mi = ms.find(cmdobject["mic"].toString());
        if (mi != ms.end())
        {
            QJsonObject mic;
            mic["name"] = mi->first;
            mic["mute"] = mi->second;
            ca.append(mic);
        }
        else
        {
            rc = STS_ERROR;
        }
    }
    else
    {
        for(const auto &m : ms)
        {
            QJsonObject mic;
            mic["name"] = m.first;
            mic["mute"] = m.second;
            ca.append(mic);
        }
    }
    r["mics"] = ca;

    r["command"] = "getmic";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_shutdown(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_SUCCESS;
    QJsonObject r;

    systemFunctions.Shutdown();

    r["command"] = "shutdown";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_snapshot(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    QJsonObject r;

    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else
    {
        int camera = cmdobject["camera"].toInt();

        if (camera < 0 || camera > 2)
        {
            qDebug() << "bad camera";
        }
        else
        {
            QString filename;
            rc = systemFunctions.Snapshot(camera,filename);
            if (rc == STS_SUCCESS)
            {
                r["filename"] = filename;
            }
        }
    }
    r["command"] = "snapshot";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_paths(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ rc = STS_SUCCESS;

    r["command"] = "paths";
    r["status"] = rc;
    r["videos"] = MainWindow::GlobalVO->SDCARD_MP4_PATH;
    r["xml"] = MainWindow::GlobalVO->XML_PATH;
    r["xmlfirst"] = MainWindow::GlobalVO->XML_FIRST_PATH;
    r["snapshot"] = MainWindow::GlobalVO->SNAPSHOT_PATH;
    r["failsafe"] = MainWindow::GlobalVO->MSATA_AVS_PATH;
    r["cache"] = CACHE_PATH;
    std::string X1_PATH{ MainWindow::GlobalVO->FOCUS_X1_PATH};
    X1_PATH += "DCIM/";
    r["focus_x1"] = X1_PATH.data();
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_space(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ rc = STS_SUCCESS;

    std::map<QString,QString> volumes {
        { "videos", MainWindow::GlobalVO->SDCARD_MP4_PATH },
        { "failsafe", MainWindow::GlobalVO->MSATA_AVS_PATH },
        { "xml", MainWindow::GlobalVO->XML_PATH }
    };
    QJsonArray vols;
    for(const auto v : volumes)
    {
        QJsonObject vo;
        QStorageInfo storage(v.second);
        vo["name"] = v.first;
        vo["available"] = storage.bytesAvailable()/(1024 * 1024);
        vo["total"] = storage.bytesTotal()/(1024 * 1024);
        vols.append(vo);
    }
    r["volumes"] = vols;
    r["command"] = "space";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_modifyevent(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ rc = STS_ERROR;

    if (cmdobject["eventname"].isString() && cmdobject["event"].isObject())
    {
        std::map<QString,QString> values;
        QJsonObject o = cmdobject["event"].toObject();
        for(auto i = o.begin(); i != o.end() ; ++i)
        {
            if (i.value().isString())
            {
                values[i.key()] = i.value().toString();
            }
        }
        rc = systemFunctions.ModifyEvent(cmdobject["eventname"].toString(),values);
    }

    r["command"] = "modifyevent";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_getevent(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ rc = STS_ERROR;

    if (cmdobject["eventname"].isString())
    {
        std::map<QString,QString> fields;
        rc = systemFunctions.GetEvent(cmdobject["eventname"].toString(),fields);
        if (rc == STS_SUCCESS)
        {
            QJsonObject eo;
            for(const auto &f : fields)
            {
                eo[f.first] = f.second;
            }
            r["event"] = eo;
        }
    }

    r["command"] = "getevent";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_bookmark(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ rc = STS_ERROR;

    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else
    {
        rc = systemFunctions.Bookmark(cmdobject["camera"].toInt());
    }

    r["command"] = "bookmark";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_eventlist(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ rc = STS_ERROR;

    std::list<QString> events;
    rc = systemFunctions.ListEvents(events);
    if (rc == STS_SUCCESS)
    {
        QJsonArray ea;
        for(const auto &e : events)
        {
           ea.append(e);
        }
        r["events"] = ea;
    }

    r["command"] = "eventlist";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pendingeventlist(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ rc = STS_SUCCESS;

    std::set<QString> events = systemFunctions.PendingEvents();
    QJsonArray ea;
    for(const auto &e : events)
    {
       ea.append(e);
    }
    r["events"] = ea;

    r["command"] = "pendingeventlist";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_serverstart(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    rc = systemInterface->ServerStart();
    sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"pm_serverstart\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
}

void TcpServer::handle_pm_serverstop(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    rc = systemInterface->ServerStop();
    sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"serverstop\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
}

void TcpServer::handle_pm_livestream(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    Status_ rc = STS_ERROR;
    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else if (! cmdobject["on"].isBool())
    {
        qDebug() << "no control";
    }
    else
    {
        rc = systemInterface->LiveStream(cmdobject["camera"].toInt(),cmdobject["on"].toBool());
    }
    sendMessage(tcpSocket,QByteArray((QString("{\"command\":\"pm_livestream\",\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
}

void TcpServer::handle_pm_liveviewstart(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ rc = STS_ERROR;
    if (!cmdobject["camera"].isDouble() ||
        !cmdobject["x"].isDouble() ||
        !cmdobject["y"].isDouble() ||
        !cmdobject["cx"].isDouble() ||
        !cmdobject["cx"].isDouble() ||
        !cmdobject["display"].isDouble())
    {
    }
    else
    {
        rc = systemInterface->LiveViewStart(
            cmdobject["x"].toInt(),
            cmdobject["y"].toInt(),
            cmdobject["cx"].toInt(),
            cmdobject["cy"].toInt(),
            cmdobject["camera"].toInt(),
            cmdobject["display"].toInt());
    }

    r["command"] = "pm_liveviewstart";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_liveviewstop(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ rc = STS_ERROR;
    if (! cmdobject["camera"].isDouble())
    {
    }
    else
    {
        rc = systemInterface->LiveViewStop(cmdobject["camera"].toInt());
    }

    r["command"] = "pm_liveviewstop";
    r["status"] = rc;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_status(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    r["command"] = "status";
    r["date"] = QDateTime::currentDateTime().toString("MM/dd/yyyy");
    r["time"] = QDateTime::currentDateTime().toString("hh:mm:ss AP");
    r["status"] = STS_SUCCESS;
    QJsonArray ca;
    for(int i = 0 ; i < MainWindow::GlobalVO->SC_camera_number ; i++)
    {
        ca.append(CameraStatus(i));
    }
    r["camera"] = ca;
    QJsonArray na;
    const std::list<SystemFunctions::NoticeEntry> &nel = systemFunctions.Notices();
    for(const auto &n : nel)
    {
        QJsonObject notice;
        notice["sequence"] = (int)n.sequence;
        notice["seconds"] = (int)n.seconds;
        notice["notice"] = n.notice;
        notice["code"] = n.code;
        na.append(notice);
    }
    r["notices"] = na;
    QJsonObject errors;
    std::map<QString,bool> errorConditions;
    systemFunctions.GetErrors(errorConditions);
    for(const auto e: errorConditions)
    {
        errors[e.first] = e.second;
    }
    r["errorconditions"] = errors;
    r["user"] = MainWindow::RecordingVO->USER_ID;
    r["officer"] = MainWindow::RecordingVO->OFFICER_ID;
    r["partner"] = MainWindow::RecordingVO->PARTNER_ID;
    r["unit"] = MainWindow::RecordingVO->PATROL_UNIT;
    r["login"] = systemFunctions.IsLogin();
    r["emergencylogin"] = systemFunctions.IsEmergencyLogin();
    r["initialized"] = systemFunctions.isInitialized();
    r["synccontrol"] = systemFunctions.IsSyncControl();

    QString sname;
    if (systemFunctions.StreamingFile(sname))
    {
        r["streamingfile"] = sname;
    }
    char buf[5];
    strncpy(buf,MainWindow::GlobalVO->WLStatus,4);
    buf[4] = '\0';
    r["wlstatus"] = buf;
    r["uploadfilename"] = MainWindow::GlobalVO->CurrentUploadFileName;
    r["uploadsize"] = MainWindow::GlobalVO->uploadSize;
    r["uploadedsize"] = MainWindow::GlobalVO->uploadedSize;
    r["filesuploaded"] = MainWindow::GlobalVO->numberOfFilesUploaded;
    r["filestoupload"] = MainWindow::GlobalVO->numberOfFilesToUpload;
    r["downloadfilename"] = MainWindow::GlobalVO->CurrentDownloadFileName;
    r["uploadpercentage"] = MainWindow::GlobalVO->CurrentUploadPercentage;
    r["downloadpercentage"] = MainWindow::GlobalVO->CurrentDownloadPercentage;
    r["uploadspeed"] = MainWindow::GlobalVO->UploadSpeed;
    r["downloadspeed"] = MainWindow::GlobalVO->DownloadSpeed;
    r["signalstrength"] = MainWindow::GlobalVO->wifiSignalStrength;
    r["accesspoint"] = MainWindow::GlobalVO->wifiAccessPoint;

    r["internalbatteryvoltage"] = MainWindow::metadata->upsVoltage;

    r["inputvoltage"] = MainWindow::GlobalVO->MCU_mvC;
    r["poweracc"] = MainWindow::GlobalVO->POWER_ACC;

    r["devicetemperature"] = MainWindow::GlobalVO->MCU_currentTemperature;
    r["gpsstatus"] = MainWindow::GlobalVO->GPSStatus;
    r["pendrivestatus"] = MainWindow::GlobalVO->PenDriveStatus;
    r["covertmode"] = systemFunctions.isCovertInterviewMode();

    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_ls(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_SUCCESS;

    if (! cmdobject["path"].isString())
    {
        qDebug() << "no path";
        status = STS_ERROR;
    }
    else
    {
        QDir dir(cmdobject["path"].toString());
        if (! dir.exists())
        {
            status = STS_ERROR;
        }
        else
        {
            if (cmdobject["filters"].isArray())
            {
                QStringList filters;
                QJsonArray fa = cmdobject["filters"].toArray();
                for(int i = 0 ; i < fa.size() ; i++)
                {
                    if (fa[i].isString())
                    {
                        filters.append(fa[i].toString());
                    }
                }
                dir.setNameFilters(filters);
                r["filters"] = fa;
            }
            if (cmdobject["sort"].isString())
            {
                if (cmdobject["sort"].toString() == "time")
                {
                    dir.setSorting(QDir::Time);
                }
                else if (cmdobject["sort"].toString() == "size")
                {
                    dir.setSorting(QDir::Size);
                }
                else if (cmdobject["sort"].toString() == "name")
                {
                    dir.setSorting(QDir::Name);
                }
            }
            if (cmdobject["reverse"].isBool())
            {
                if (cmdobject["reverse"].toBool())
                {
                    dir.setSorting(QDir::Reversed | dir.sorting());
                }
            }
            QStringList l = dir.entryList();
            QJsonArray names;

            for(auto it = l.begin() ; it != l.end() ; ++it) {
                qDebug() << *it;
                names.append(*it);
            }
            r["files"] = names;
        }
    }
    r["command"] = "ls";
    r["path"] = cmdobject["path"].toString();
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_network(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_SUCCESS;

    bool all = false;
    if (cmdobject["all"].isBool())
    {
        all = cmdobject["all"].toBool();
    }

    if (all)
    {
        QJsonArray ia;
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for(const auto &i : interfaces)
        {
            QJsonObject io;
            io["name"] = i.name();
            io["MAC"] = i.hardwareAddress();
            io["up"] = bool(i.flags() & QNetworkInterface::IsUp);
            if (io["up"].toBool())
            {
                QList<QNetworkAddressEntry> entries = i.addressEntries();
                QJsonArray aa;
                for(const auto &a : entries)
                {
                    QJsonObject ao;
                    ao["ip"] = a.ip().toString();
                    ao["mask"] = a.netmask().toString();
                    aa.append(ao);
                }
                io["addresses"] = aa;
            }
            ia.append(io);
        }
        r["interfaces"] = ia;
    }

    QString wlanip = "";
    QString wlanmask = "";
    GetWlanIPAndMask(wlanip,wlanmask);
    r["ip"] = wlanip;
    r["netmask"] = wlanmask;

    if (all)
    {
        QJsonArray ra;
        QFile rfile("/proc/net/route");
        if (rfile.open(QIODevice::ReadOnly| QIODevice::Text))
        {
           QTextStream in(&rfile);
           // while (!in.atEnd())
           QString line;
           while (! (line = in.readLine()).isNull())
           {
               QStringList list = line.split(QRegExp("\\s+"));

               if (list.size() >= 8 && list[0] != "Iface")
               {
                   QJsonObject ro;
                   ro["interface"] = list[0];
                   ro["destination"] = ntoa(Qstrtol(list[1],16));
                   ro["gateway"] = ntoa(Qstrtol(list[2],16));
                   ro["mask"] = ntoa(Qstrtol(list[7],16));
                   ra.append(ro);
               }
           }
           rfile.close();
        }
        r["routes"] = ra;
    }
    r["gatewayip"] = GetWlanGateway();

    // TBD, is there a better way?
    QJsonArray ns;
    std::list<QString> dnslist = GetNameservers();
    for(auto de : dnslist)
    {
        ns.append(de);
    }
    r["nameservers"] = ns;

    QJsonObject wifi;
    wifi["SSID"] = GetActiveSSID();
    r["wifi"] = wifi;

    QJsonObject config;
    config["aws"] = MainWindow::GlobalVO->SC_AWS;
    config["awsoption"] = MainWindow::GlobalVO->SC_AWSOption;

    config["SSID"] = MainWindow::GlobalVO->SSIDName;

    config["uploaduri"] = MainWindow::GlobalVO->SC_UploadURI;
    config["downloaduri"] = MainWindow::GlobalVO->SC_DownloadURI;
    config["wsuri"] = MainWindow::GlobalVO->SC_WSURI;
    config["soapname"] = MainWindow::GlobalVO->SC_SOAPName;
    r["config"] = config;

    r["command"] = "network";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_init(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_ERROR;

    status = systemFunctions.isInitialized() ? STS_SUCCESS : STS_ERROR;

    r["command"] = "init";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_gps(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_SUCCESS;

    r["latitude"] = MainWindow::GlobalVO->GPSLatitude;
    r["longitude"] = MainWindow::GlobalVO->GPSLongitude;
    r["altitude"] = MainWindow::metadata->altitude;
    r["speed"] = MainWindow::metadata->gps_speed;
    r["track"] = MainWindow::metadata->gps_track;
    r["time"] = MainWindow::GlobalVO->GPSTime;
    r["satellites"] = MainWindow::metadata->gps_satellites;
    r["lock"] = MainWindow::metadata->gps_mode;

    r["command"] = "gps";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_login(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    QString errormsg;
    Status_ status = STS_ERROR;
    if (! cmdobject["officer"].isString())
    {
        errormsg = "no officer";
    }
    else if (! cmdobject["password"].isString())
    {
        errormsg = "no password";
    }
    else
    {
        QString partner;
        QString unit;
        if (cmdobject["partner"].isString())
        {
            partner = cmdobject["partner"].toString();
        }
        if (cmdobject["unit"].isString())
        {
            unit = cmdobject["unit"].toString();
        }
        if (systemFunctions.Login(cmdobject["officer"].toString(),cmdobject["password"].toString(),partner,unit,errormsg))
        {
            status = STS_SUCCESS;
        }
    }
    r["command"] = "login";
    r["status"] = status;
    r["errormsg"] = errormsg;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_logout(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    QString errormsg;
    Status_ status = STS_SUCCESS;

    systemFunctions.Logout();

    r["command"] = "logout";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_streamfile(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_SUCCESS;
    QString errormsg;
    if (! cmdobject["filename"].isString())
    {
        errormsg = "no filename";
    }
    else
    {
        // TBD, remove this debug
        qDebug() << "TcpServer::handle_streamfile start: " << cmdobject["filename"].toString();
        QString urlpath;
        status = systemFunctions.StreamFile(cmdobject["filename"].toString(),urlpath);
        if (status == STS_SUCCESS)
        {
            r["urlpath"] = urlpath;
        }
    }
    r["command"] = "streamfile";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_upload(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_SUCCESS;

    if (cmdobject["icv"].isBool())
    {
        if (cmdobject["icv"].toBool())
        {
            status = systemInterface->StartTransfer();
        }
        else
        {
            status = systemInterface->StopTransfer();
        }
    }
    if (cmdobject["bwc"].isBool())
    {
        if (cmdobject["bwc"].toBool())
        {
            status = systemInterface->StartTransfer();
        }
        else
        {
            status = systemInterface->StopTransfer();
        }
    }

    r["command"] = "upload";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_sound(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_SUCCESS;

    MainWindow::sound.play();

    r["command"] = "sound";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_trigger(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_ERROR;

    if (cmdobject["code"].isDouble())
    {
       systemFunctions.HandleTrigger(cmdobject["code"].toInt());
       status = STS_SUCCESS;
    }

    r["command"] = "trigger";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_version(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_SUCCESS;

    std::map<QString,QString> versions;
    SystemFunctions::Versions(versions);

    for(auto v : versions)
    {
        r[v.first] = v.second;
    }

    r["command"] = "version";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_volume(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    Status_ status = STS_SUCCESS;

    if (! cmdobject["device"].isString())
    {
        QJsonArray va;
        QJsonObject v;

        v["device"] = "wmic1";
        v["percent"] = systemFunctions.Volume("wmic1");
        va.append(v);

        v["device"] = "wmic2";
        v["percent"] = systemFunctions.Volume("wmic2");
        va.append(v);

        v["device"] = "speaker";
        v["percent"] = systemFunctions.Volume("speaker");
        va.append(v);

        r["volumes"] = va;
    }
    else
    {
        if (! cmdobject["percent"].isDouble())
        {
            status = STS_ERROR;
        }
        else
        {
            if (! systemFunctions.Volume(cmdobject["device"].toString(),cmdobject["percent"].toInt()))
            {
                status = STS_ERROR;
            }
        }
    }

    r["command"] = "volume";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_initpool(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    QString errormsg;
    Status_ status = STS_ERROR;

    if (! cmdobject["size"].isDouble())
    {
        errormsg = "no size";
    }
    else
    {
       status = systemInterface->MemInitpool(cmdobject["size"].toInt());
    }
    r["command"] = "pm_initpool";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::handle_pm_recordinitcam(QTcpSocket *tcpSocket,QJsonObject &cmdobject)
{
    QJsonObject r;
    QString errormsg;
    Status_ status = STS_ERROR;

    Status_ rc = STS_ERROR;
    if (! cmdobject["camera"].isDouble())
    {
        qDebug() << "no camera";
    }
    else
    {
        // Record_InitCamera(1920, 1080, 30, 30, 1, 6000000, 0, 90, 90, 0, 0);
        int width = 1920;
        int height = 1080;
        int fps = 30;
        int gop = 30;
        int controlrate = 2;
        int quality = 1;
        int bitrate = 6000000;
        int buffersize = 90;
        int camera_id = cmdobject["camera"].toInt();
        int audio_id = camera_id;

        if (cmdobject["width"].isDouble()) width = cmdobject["width"].toInt();
        if (cmdobject["height"].isDouble()) height = cmdobject["height"].toInt();
        if (cmdobject["fps"].isDouble()) fps = cmdobject["fps"].toInt();
        if (cmdobject["gop"].isDouble()) gop = cmdobject["gop"].toInt();
        if (cmdobject["controlrate"].isDouble()) controlrate = cmdobject["controlrate"].toInt();
        if (cmdobject["quality"].isDouble()) quality = cmdobject["quality"].toInt();
        if (cmdobject["bitrate"].isDouble()) bitrate = cmdobject["bitrate"].toInt();
        if (cmdobject["buffersize"].isDouble()) buffersize = cmdobject["buffersize"].toInt();
        if (cmdobject["audioid"].isDouble()) audio_id = cmdobject["audioid"].toInt();

        status = systemInterface->RecordInitCam(
            width,
            height,
            fps,
            gop,
            controlrate,
            bitrate,
            quality,
            buffersize,
            camera_id,
            audio_id
            );
    }

    r["command"] = "recordinitcam";
    r["status"] = status;
    QJsonDocument rd(r);
    sendMessage(tcpSocket,rd.toJson());
}

void TcpServer::processJsonMessage(QTcpSocket *tcpSocket,QByteArray &message)
{
    qDebug() << "Got tcp message size=" << message.size() << " : " << qPrintable(message);

    QJsonDocument cmd(QJsonDocument::fromJson(message));
    if (cmd.isNull())
    {
        qDebug() << "Bad JSON message " << message;
        return;
    }

    QJsonObject cmdobject = cmd.object();
    if (! cmdobject.contains("command"))
    {
        Status_ rc = STS_ERROR;
        qDebug() << "No command " << message;
        sendMessage(tcpSocket,QByteArray((QString("{\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
        return;
    }
    if (! cmdobject["command"].isString())
    {
        Status_ rc = STS_ERROR;
        qDebug() << "Command not string" << message;
        sendMessage(tcpSocket,QByteArray((QString("{\"status\":") + QVariant(rc).toString() + "}").toUtf8()));
        return;
    }

    // connection manager commands
    if (cmdobject["command"] == "cm_starttransfer") handle_cm_starttransfer(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "cm_stoptransfer") handle_cm_stoptransfer(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "cm_startx1import") handle_cm_startx1import(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "cm_stopx1import") handle_cm_stopx1import(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "cm_remakeconnection") handle_cm_remakeconnection(tcpSocket,cmdobject);

    // metadata manager commands
    else if (cmdobject["command"] == "mm_wmicenable") handle_mm_wmicenable(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "mm_wmicdisable") handle_mm_wmicdisable(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "mm_wmiccoverton") handle_mm_wmiccoverton(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "mm_wmiccovertoff") handle_mm_wmiccovertoff(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "mm_covertinterviewon") handle_mm_covertinterviewon(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "mm_covertinterviewoff") handle_mm_covertinterviewoff(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "mm_wmicon") handle_mm_wmicon(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "mm_wmicoff") handle_mm_wmicoff(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "mm_speakermuteon") handle_mm_speakermuteon(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "mm_speakermuteoff") handle_mm_speakermuteoff(tcpSocket,cmdobject);

    // playback manager commands
    else if (cmdobject["command"] == "pm_fileinfo") handle_pm_fileinfo(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_initpool") handle_pm_initpool(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_livestream") handle_pm_livestream(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_liveviewstart") handle_pm_liveviewstart(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_liveviewstop") handle_pm_liveviewstop(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_recordinitcam") handle_pm_recordinitcam(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_setosdcontent") handle_pm_setosdcontent(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_setosdstats") handle_pm_setosdstats(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_serverstart") handle_pm_serverstart(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_serverstop") handle_pm_serverstop(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_snapshot") handle_pm_snapshot(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_streamstartfile") handle_pm_streamstartfile(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_streamfileduration") handle_pm_streamfileduration(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_streamstopfile") handle_pm_streamstopfile(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_startrecordmp4") handle_pm_startrecordMP4(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_stoprecordmp4") handle_pm_stoprecordMP4(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_startrecordts") handle_pm_startrecordTS(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_stoprecordts") handle_pm_stoprecordTS(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_recsyncnextmp4") handle_pm_recsyncnextMP4(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pm_recsyncnextts") handle_pm_recsyncnextTS(tcpSocket,cmdobject);

    // "high level" commands
    else if (cmdobject["command"] == "getevent") handle_getevent(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "modifyevent") handle_modifyevent(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "bookmark") handle_bookmark(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "eventlist") handle_eventlist(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "pendingeventlist") handle_pendingeventlist(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "init") handle_init(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "gps") handle_gps(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "ls") handle_ls(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "login") handle_login(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "logout") handle_logout(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "network") handle_network(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "paths") handle_paths(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "ping") handle_ping(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "readfile") handle_readfile(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "record") handle_record(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "setmic") handle_setmic(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "getmic") handle_getmic(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "shutdown") handle_shutdown(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "snapshot") handle_snapshot(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "sound") handle_sound(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "space") handle_space(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "status") handle_status(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "stoprecord") handle_stoprecord(tcpSocket,cmdobject);
    // else if (cmdobject["command"] == "switch") handle_switch(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "streamfile") handle_streamfile(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "upload") handle_upload(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "trigger") handle_trigger(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "version") handle_version(tcpSocket,cmdobject);
    else if (cmdobject["command"] == "volume") handle_volume(tcpSocket,cmdobject);
    else
    {
        qDebug() << "Unknown commmand \"" << cmdobject["command"].toString() << "\"";
        sendMessage(tcpSocket,QByteArray((QString("{\"status\":") + QVariant(STS_ERROR).toString() + "}").toUtf8()));
    }
}
