#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QtCore>
#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QDebug>

#include "gui_common.h"

enum TCPMessageType {
    tmt_JSON = 0,
    tmt_BINARY = 1,
};

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = 0,int port = 9999);

public slots:
    void tcpNewConnection();
    void tcpReadyRead();
    void tcpDisconnected();

private:
    QTcpServer *tcpServer = nullptr;
    QHash<QTcpSocket *, QByteArray *> tcpBuffers;

    void processTcpMessage(QTcpSocket *,QByteArray &);
    void processJsonMessage(QTcpSocket *,QByteArray &);
    void processBinaryMessage(QTcpSocket *,QByteArray &,bool);
    int sendMessage(QTcpSocket *,const QByteArray &,TCPMessageType = tmt_JSON,bool = false);

    //
    // We handle calls that tranlate to bare playback manager calls
    // as well as ones that are higher level, meant for external clients
    //
    // fist the simple, bare ones
    void handle_cm_starttransfer(QTcpSocket *,QJsonObject &);
    void handle_cm_stoptransfer(QTcpSocket *,QJsonObject &);
    void handle_cm_startx1import(QTcpSocket *,QJsonObject &);
    void handle_cm_stopx1import(QTcpSocket *,QJsonObject &);
    void handle_cm_remakeconnection(QTcpSocket *,QJsonObject &);

    void handle_mm_wmicenable(QTcpSocket *,QJsonObject &);
    void handle_mm_wmicdisable(QTcpSocket *,QJsonObject &);
    void handle_mm_wmiccoverton(QTcpSocket *,QJsonObject &);
    void handle_mm_wmiccovertoff(QTcpSocket *,QJsonObject &);
    void handle_mm_wmicon(QTcpSocket *,QJsonObject &);
    void handle_mm_wmicoff(QTcpSocket *,QJsonObject &);
    void handle_mm_covertinterviewon(QTcpSocket *,QJsonObject &);
    void handle_mm_covertinterviewoff(QTcpSocket *,QJsonObject &);
    void handle_mm_speakermuteon(QTcpSocket *,QJsonObject &);
    void handle_mm_speakermuteoff(QTcpSocket *,QJsonObject &);

    void handle_pm_fileexportstart(QTcpSocket *,QJsonObject &);
    void handle_pm_fileexportstop(QTcpSocket *,QJsonObject &);
    void handle_pm_fileexportstatus(QTcpSocket *,QJsonObject &);
    void handle_pm_fileinfo(QTcpSocket *,QJsonObject &);
    void handle_pm_flushrecord(QTcpSocket *,QJsonObject &);

    void handle_pm_initpool(QTcpSocket *,QJsonObject &);

    void handle_pm_livestream(QTcpSocket *,QJsonObject &);
    void handle_pm_liveviewstart(QTcpSocket *,QJsonObject &);
    void handle_pm_liveviewstop(QTcpSocket *,QJsonObject &);

    void handle_pm_playfile(QTcpSocket *,QJsonObject &);
    void handle_pm_playpause(QTcpSocket *,QJsonObject &);
    void handle_pm_playstop(QTcpSocket *,QJsonObject &);
    void handle_pm_playgetposition(QTcpSocket *,QJsonObject &);
    void handle_pm_playsetrate(QTcpSocket *,QJsonObject &);
    void handle_pm_playclosefile(QTcpSocket *,QJsonObject &);
    void handle_pm_playwaiteos(QTcpSocket *,QJsonObject &);

    void handle_pm_recsyncnextTS(QTcpSocket *,QJsonObject &);
    void handle_pm_recsyncnextMP4(QTcpSocket *,QJsonObject &);
    void handle_pm_recordinitcam(QTcpSocket *,QJsonObject &);

    void handle_pm_setaudiovolume(QTcpSocket *,QJsonObject &);
    void handle_pm_setosdcontent(QTcpSocket *,QJsonObject &);
    void handle_pm_setosdstats(QTcpSocket *,QJsonObject &);
    void handle_pm_startrecordMP4(QTcpSocket *,QJsonObject &);
    void handle_pm_stoprecordMP4(QTcpSocket *,QJsonObject &);
    void handle_pm_startrecordTS(QTcpSocket *,QJsonObject &);
    void handle_pm_stoprecordTS(QTcpSocket *,QJsonObject &);
    void handle_pm_streamfileduration(QTcpSocket *,QJsonObject &);
    void handle_pm_streamstartfile(QTcpSocket *,QJsonObject &);
    void handle_pm_streamstopfile(QTcpSocket *,QJsonObject &);
    void handle_pm_serverstart(QTcpSocket *,QJsonObject &);
    void handle_pm_serverstop(QTcpSocket *,QJsonObject &);
    void handle_pm_snapshot(QTcpSocket *,QJsonObject &);
    void handle_pm_switch(QTcpSocket *,QJsonObject &);

    //
    // The higher level ones.
    //
    void handle_getevent(QTcpSocket *,QJsonObject &);
    void handle_bookmark(QTcpSocket *,QJsonObject &);
    void handle_eventlist(QTcpSocket *,QJsonObject &);
    void handle_pendingeventlist(QTcpSocket *,QJsonObject &);
    void handle_gps(QTcpSocket *,QJsonObject &);
    void handle_init(QTcpSocket *,QJsonObject &);
    void handle_login(QTcpSocket *,QJsonObject &);
    void handle_logout(QTcpSocket *,QJsonObject &);
    void handle_ls(QTcpSocket *,QJsonObject &);
    void handle_modifyevent(QTcpSocket *,QJsonObject &);
    void handle_network(QTcpSocket *,QJsonObject &);
    void handle_paths(QTcpSocket *,QJsonObject &);
    void handle_ping(QTcpSocket *,QJsonObject &);
    void handle_readfile(QTcpSocket *,QJsonObject &);
    void handle_record(QTcpSocket *,QJsonObject &);
    void handle_setmic(QTcpSocket *,QJsonObject &);
    void handle_getmic(QTcpSocket *,QJsonObject &);
    void handle_shutdown(QTcpSocket *,QJsonObject &);
    void handle_space(QTcpSocket *,QJsonObject &);
    void handle_snapshot(QTcpSocket *,QJsonObject &);
    void handle_status(QTcpSocket *,QJsonObject &);
    void handle_stoprecord(QTcpSocket *,QJsonObject &);
    void handle_streamfile(QTcpSocket *,QJsonObject &);
    void handle_upload(QTcpSocket *,QJsonObject &);
    void handle_version(QTcpSocket *,QJsonObject &);
    void handle_volume(QTcpSocket *,QJsonObject &);

    // testing
    void handle_sound(QTcpSocket *,QJsonObject &);
    void handle_trigger(QTcpSocket *,QJsonObject &);
};

#endif // TCPSERVER_H

