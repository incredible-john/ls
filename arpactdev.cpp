#include "arpactdev.h"
#include "ui_arpactdev.h"

arpactdev::arpactdev(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::arpactdev)
{
    ui->setupUi(this);
    ip_addr = (char *)malloc(sizeof(char) * 16); //申请内存存放IP地址
    ip_netmask = (char *)malloc(sizeof(char) * 16); //申请内存存放NETMASK地址
    ip_mac = (unsigned char *)malloc(sizeof(unsigned char) * 6); //申请内存存放MAC地址

    dev_num = 0; //初始化适配器数量为0

    //初始化线程 中的arpinf 指针
    arpth.arpinf = this;
    arpgth.arpinf = this;
    checknetstate.arpinf = this;

    flag = false;               //初始化时ip地址不可用
    if(connect(&arpth,SIGNAL(sendall()),&arpgth,SLOT(sendallarp())))       //将发送arp完的信号和接受arp线程中槽函数绑定实现发送完结束接收线程
    {
        qDebug()<<"关联成功";
    }

    if(connect(&arpgth,SIGNAL(sendactmac(ulong,QString)),this,SLOT(getactmac(ulong,QString)))){               //将探测到新mac 与更新活动主机mac 相关联
        qDebug()<<"关联成功";
    }

    if(connect(&checknetstate,SIGNAL(updev_tip()),this,SLOT(updatedev_tip()))){
        qDebug()<<"关联成功";
    }
    this->init();

    ui->tcpport->setValidator(new QIntValidator(1234,3356,this));   //设置tcpport输入窗口的数据限制范围

    this->tcpServer = new QTcpServer(this);                   //实例化tcpserver对象
}

arpactdev::~arpactdev()
{
    delete ui;
    delete ip_addr;
    delete ip_netmask;
    delete ip_mac;
    delete tcpServer;
    arpth.terminate();
    arpth.wait();
    arpgth.terminate();
    arpgth.wait();
    checknetstate.terminate();
    checknetstate.wait();
}

int arpactdev::init()
{



    //获取本地适配器列表
    if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1) {
        //结果为-1代表出现获取适配器列表失败
        qDebug()<<"Error in pcap_findalldevs_ex:\n";
        //exit(0)代表正常退出,exit(other)为非正常退出,这个值会传给操作系统
        exit(1);
    }
    for (d = alldevs; d != NULL; d = d->next) {
        qDebug()<<"-----------------------------------------------------------------\nnumber:"<<++dev_num<<"\nname:"<<d->name<<"\n";
        if (d->description) {
            //打印适配器的描述信息
            qDebug()<<"description:"<<d->description<<"\n";

        }
        else {
            //适配器不存在描述信息
            qDebug()<<"description:no description\n";

        }
        //打印本地环回地址
        qDebug()<<"\tLoopback:"<<((d->flags & PCAP_IF_LOOPBACK) ? "yes" : "no");
        /**
        pcap_addr *  next     指向下一个地址的指针
        sockaddr *  addr       IP地址
        sockaddr *  netmask  子网掩码
        sockaddr *  broadaddr   广播地址
        sockaddr *  dstaddr        目的地址
        */
        pcap_addr_t *a;       //网络适配器的地址用来存储变量
        for (a = d->addresses; a; a = a->next) {

            //sa_family代表了地址的类型,是IPV4地址类型还是IPV6地址类型
            switch (a->addr->sa_family)
            {
            case AF_INET:  //代表IPV4类型地址

                qDebug()<<"Address Family Name:AF_INET";
                if (a->addr) {
                    //->的优先级等同于括号,高于强制类型转换,因为addr为sockaddr类型，对其进行操作须转换为sockaddr_in类型
                    qDebug()<<"Address:";
                    QString ip= iptos(((struct sockaddr_in *)a->addr)->sin_addr.s_addr);
                    qDebug()<<ip;

                    usabledev.append(d);    // 添加可用的适配器
                    ui->ipcombox->addItem(ip);  //添加IP地址选择项

                }
                if (a->netmask) {
                    qDebug()<<"Netmask:";
                    qDebug()<<iptos(((struct sockaddr_in *)a->netmask)->sin_addr.s_addr);

                }
                if (a->broadaddr) {
                    qDebug()<<"Broadcast Address: ";
                    qDebug()<<iptos(((struct sockaddr_in *)a->broadaddr)->sin_addr.s_addr);
                }
                if (a->dstaddr) {
                    qDebug()<<"Destination Address:";
                    qDebug()<<iptos(((struct sockaddr_in *)a->dstaddr)->sin_addr.s_addr);

                }
                break;
            case AF_INET6: //代表IPV6类型地址
                qDebug()<<"Address Family Name:AF_INET6\n"<<"this is an IPV6 address\n";

                break;
            default:
                break;
            }
        }
    }
    //i为0代表上述循环未进入,即没有找到适配器,可能的原因为Winpcap没有安装导致未扫描到
    if (dev_num == 0) {
        ui->dev_tip->setText("没有找到适配器");
        qDebug()<<"interface not found,please check winpcap installation";
    }
    else
    {
        if(!usabledev[0])
        {
            //代表没有可用的ipv4 地址
            ui->dev_tip->setText("没有可用的ipv4地址");
            pcap_freealldevs(alldevs);
            return 1;
        }
        else
        {
            //有可用ipv4 移除选项中的无
            ui->ipcombox->removeItem(0);
        }


    }
    return 0;
}

QString arpactdev::iptos(u_long in)
{
    QString output;

    u_char *p;

    p = (u_char *)&in;
    output = QString("%1.%2.%3.%4").arg(p[0]).arg(p[1]).arg(p[2]).arg(p[3]);
    return output;
}

QString arpactdev::mactos(unsigned char* mac){
    return QString("%1-%2-%3-%4-%5-%6").arg(mac[0],2,16).arg(mac[1],2,16).arg(mac[2],2,16).arg(mac[3],2,16).arg(mac[4],2,16).arg(mac[5],2,16);
}
void arpactdev::ifget(pcap_if_t *d, char *ip_addr, char *ip_netmask,unsigned int &unetmask)
{
    pcap_addr_t *a;
    //遍历所有的地址,a代表一个pcap_addr
    for (a = d->addresses; a; a = a->next) {
        switch (a->addr->sa_family) {
        case AF_INET:  //sa_family ：是2字节的地址家族，一般都是“AF_xxx”的形式。通常用的都是AF_INET。代表IPV4
            if (a->addr) {
                QString ipstr;
                //将地址转化为字符串
                ipstr = iptos(((struct sockaddr_in *) a->addr)->sin_addr.s_addr); //*ip_addr
                memcpy(ip_addr, ipstr.toLatin1().data(), 16);
            }
            if (a->netmask) {
                QString netmaskstr;
                netmaskstr = iptos(((struct sockaddr_in *) a->netmask)->sin_addr.s_addr);
                unetmask = ((struct sockaddr_in *) a->netmask)->sin_addr.s_addr;
                memcpy(ip_netmask, netmaskstr.toLatin1().data(), 16);
            }
        case AF_INET6:
            break;
        }
    }
}

int arpactdev::GetSelfMac(pcap_t *adhandle, const char *ip_addr, unsigned char *ip_mac)
{
    unsigned char sendbuf[42]; //arp包结构大小
    int i = -1;
    int res;
    EthernetHeader eh; //以太网帧头
    Arpheader ah;  //ARP帧头
    struct pcap_pkthdr * pkt_header;//数据包头
    const u_char * pkt_data;//数据
    //将已开辟内存空间 eh.dest_mac_add 的首 6个字节的值设为值 0xff。
    memset(eh.DestMAC, 0xff, 6); //目的地址为全为广播地址
    memset(eh.SourMAC, 0x0f, 6);
    memset(ah.DestMacAdd, 0x0f, 6);
    memset(ah.SourceMacAdd, 0x00, 6);
    //htons将一个无符号短整型的主机数值转换为网络字节顺序
    eh.EthType = htons(ETH_ARP);
    ah.HardwareType = htons(ARP_HARDWARE);
    ah.ProtocolType = htons(ETH_IP);
    ah.HardwareAddLen = 6;
    ah.ProtocolAddLen = 4;
    ah.SourceIpAdd = inet_addr("100.100.100.100");
    ah.OperationField = htons(ARP_REQUEST);
    ah.DestIpAdd = inet_addr(ip_addr);
    memset(sendbuf, 0, sizeof(sendbuf));
    memcpy(sendbuf, &eh, sizeof(eh));
    memcpy(sendbuf + sizeof(eh), &ah, sizeof(ah));


    if (pcap_sendpacket(adhandle, sendbuf, 42) == 0) {
        qDebug()<<"PacketSend succeed\n";
    }
    else {
        qDebug()<<"PacketSendPacket in getmine Error: "<<GetLastError();
        return 0;
    }

    //从interface或离线记录文件获取一个报文
    int j=0;
    while ((res = pcap_get(adhandle, &pkt_header, &pkt_data)) >= 0) {
        j++;
        qDebug()<<j;
//        if(j>20)                //认为获取mac 地址失败
//        {
//            ui->dev_tip->setText("mac 地址获取失败");
//            return 3;
//        }
        if(res==0)
        {
            qDebug()<<j;
            ui->dev_tip->setText("网络不可用");
            this->flag = false;
            return 2;//返回2代表网络无连接
        }
        if (*(unsigned short *)(pkt_data + 12) == htons(ETH_ARP)
            && *(unsigned short*)(pkt_data + 20) == htons(ARP_REPLY)
            && *(unsigned long*) (pkt_data + 38) == inet_addr("100.100.100.100")) {
            ArpPacket *recv = (ArpPacket *) pkt_data;

            for (i = 0; i < 6; i++) {
                ip_mac[i] = *(unsigned char *)(pkt_data + 22 + i);

            }
            qDebug()<<"获取自己主机的MAC地址成功!\n";
           
            break;
        }
    }
    qDebug()<<j;

    if (i == 6) {
        this->flag = true;
        return 0;
    }
    else {
        return 1;
    }
}

void arpactdev::on_ipcombox_currentIndexChanged(const QString &arg1)
{

    //让用户选择适配器进行抓包
    int num=ui->ipcombox->currentIndex();

    if(usabledev[0])
    {
        //在有可用的ip地址的情况下

        //跳转到选中的适配器
        d = usabledev[num];
//        if(adhandle!= NULL)
//            pcap_close(adhandle);
        if(newadhandle()){

            updatedev_tip();
            if(!checknetstate.isRunning()){
                checknetstate.start();
            }
        }
    }

}


bool arpactdev::newadhandle()
{
    QMutexLocker locker(&mutex);                //线程锁
    if ((adhandle = pcap_open(d->name,        //设备名称
        65535,       //存放数据包的内容长度
        PCAP_OPENFLAG_PROMISCUOUS,  //混杂模式
        100,           //超时时间
        NULL,          //远程验证
        errbuf         //错误缓冲
    )) == NULL) {
        //打开适配器失败,打印错误并释放适配器列表
        qDebug()<<stderr<<"\nUnable to open the adapter. %s is not supported by WinPcap\n"<<d->name;

        // 释放设备列表
        pcap_freealldevs(alldevs);
        return false;

    }
    unsigned int tem_netmask;

    ifget(d, ip_addr, ip_netmask,tem_netmask); //获取所选网卡的基本信息--掩码--IP地址
    struct bpf_program fcode;
    if(pcap_compile(adhandle,&fcode,"arp",1,tem_netmask)<0)
    {
        qDebug()<<"设置过滤出错";
    }
    if(pcap_setfilter(adhandle,&fcode)<0){
        qDebug()<<"设置过滤出错";
    }


    return true;
}


void arpactdev::updatedev_tip()
{
    qDebug()<<"updatedev_tip";
    QString tem = QString("%1%2%3%4").arg("ip地址:").arg(ip_addr).arg("\n子网掩码:").arg(ip_netmask);
    if(GetSelfMac(adhandle, ip_addr, ip_mac)==0)
    {
        tem += QString("\nmac地址:%1-%2-%3-%4-%5-%6").arg(ip_mac[0],2,16).arg(ip_mac[1],2,16).arg(ip_mac[2],2,16).arg(ip_mac[3],2,16).arg(ip_mac[4],2,16).arg(ip_mac[5],2,16);
        ui->dev_tip->setText(tem);

    }
}

void arpactdev::nonetwork(bool t)
{
    qDebug()<<t;
    if(t){
        ui->dev_tip->setText(QString("网络不可用"));
        flag = false;
    }else{
        QString tem = QString("%1%2%3%4").arg("ip地址:").arg(ip_addr).arg("\n子网掩码:").arg(ip_netmask);
        tem += QString("\nmac地址:%1-%2-%3-%4-%5-%6").arg(ip_mac[0],2,16).arg(ip_mac[1],2,16).arg(ip_mac[2],2,16).arg(ip_mac[3],2,16).arg(ip_mac[4],2,16).arg(ip_mac[5],2,16);
        ui->dev_tip->setText(tem);
        flag = true;
    }

}
void arpactdev::on_label_linkActivated(const QString &link)
{

}

void arpactdev::on_getmacbutton_clicked()
{


    if(!arpgth.isRunning())
        arpgth.start();                     //接收arp线程

    if(!arpth.isRunning())
        arpth.start();                  //发送arp线程

}

void arpactdev::getactmac(unsigned long ip,QString mac)
{
    actdevinf tem;
    tem.mac = mac;
    tem.tcpconsta = false;
    if(actmac.contains(iptos(ip))){
        actmac.value(iptos(ip),tem);
    }else{
        actmac.insert(iptos(ip),tem);
    }

}

QString arpactdev::actmackey(QString t)
{
    QHash<QString, actdevinf>::const_iterator i = actmac.constBegin();
    while (i != actmac.constEnd()) {
        if(i.value().mac == t)
        {
            return i.key();
        }
        ++i;
    }
    return QString("");
}


void arpactdev::on_pushButton_clicked()
{

//    upmactab();

}


void arpactdev::upmactab()
{

    unsigned char mac[6];
    QString tem;
    ui->mactableview->setRowCount(actmac.count());
    QHash<QString, actdevinf>::const_iterator i = actmac.constBegin();
    int j = 0;
    while (i != actmac.constEnd()) {
        qDebug()<<"hfhhh";
        tem = i.value().mac;
        memcpy(mac,(unsigned char*)tem.toLatin1().data(),6);    //将qstring 转换成 unsigned char
        ui->mactableview->setItem(j,0,new QTableWidgetItem(i.key()));
        ui->mactableview->setItem(j,1,new QTableWidgetItem(mactos(mac)));
        if(i.value().tcpconsta){
            ui->mactableview->setItem(j,2,new QTableWidgetItem("已连接"));
        }else{
            ui->mactableview->setItem(j,2,new QTableWidgetItem("未连接"));
        }
        ++i;
        ++j;
    }
}

void arpactdev::on_listentcpport_clicked()
{
    if(!this->tcpServer->isListening()){
        if(!this->tcpServer->listen(QHostAddress(QString(ip_addr)),ui->tcpport->text().toInt()))
        {
            qDebug() << this->tcpServer->errorString();
        }else{
            qDebug()<<this->tcpServer->serverAddress();
            qDebug()<<this->tcpServer->serverPort();
        }
    }else{
        this->tcpServer->close();   //如果正在监听则关闭
        if(!this->tcpServer->listen(QHostAddress(QString(ip_addr)),ui->tcpport->text().toInt()))
        {
            qDebug() << this->tcpServer->errorString();
        }else{
            qDebug()<<this->tcpServer->serverAddress();
            qDebug()<<this->tcpServer->serverPort();
        }
    }

}

int arpactdev::pcap_get(pcap_t *thandle, pcap_pkthdr **pcap_h, const u_char **pcap_d)
{
    QMutexLocker locker(&mutex);                //线程锁
    return pcap_next_ex(thandle,pcap_h,pcap_d);
}

void arpactdev::on_ipcombox_currentIndexChanged(int index)
{

}
