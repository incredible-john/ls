#include "arpgetthread.h"
#include "arpactdev.h"

arpgetthread::arpgetthread()
{
    stopped = false;

}

void arpgetthread::run()
{
    int res;
    struct pcap_pkthdr * pkt_header;//数据包头
    const u_char * pkt_data;//数据




    unsigned char sendbuf[42]; //arp包结构大小
    EthernetHeader eh; //以太网帧头
    Arpheader ah;  //ARP帧头
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
    ah.DestIpAdd = inet_addr(arpinf->ip_addr);
    memset(sendbuf, 0, sizeof(sendbuf));
    memcpy(sendbuf, &eh, sizeof(eh));
    memcpy(sendbuf + sizeof(eh), &ah, sizeof(ah));


    bool sendflag=false;                    //标志位
    while(true){
        if((res = arpinf->pcap_get(arpinf->adhandle,&pkt_header,&pkt_data)) >0){
            //活动主机的arp回复包
            if(*(unsigned short *) (pkt_data+12)==htons(ETH_ARP)
                    &&*(unsigned short *)(pkt_data + 20) == htons(ARP_REPLY)
                    &&*(unsigned long *)(pkt_data + 38) == inet_addr(arpinf->ip_addr)){

                ArpPacket *recv = (ArpPacket *) pkt_data;
                QString tem = mactoqstring((char *)(pkt_data+22));              //将unsigned char 转换成 qstring
                qDebug()<<recv->ah.SourceMacAdd[0]<<recv->ah.SourceMacAdd[1]<<recv->ah.SourceMacAdd[2]<<recv->ah.SourceMacAdd[3]<<recv->ah.SourceMacAdd[4]<<recv->ah.SourceMacAdd[5];
                emit sendactmac(recv->ah.SourceIpAdd,tem);
            }

            //主机回复 100.100.100.100 的包
            if (*(unsigned short *)(pkt_data + 12) == htons(ETH_ARP)
                && *(unsigned short*)(pkt_data + 20) == htons(ARP_REPLY)
                && *(unsigned long*) (pkt_data + 38) == inet_addr("100.100.100.100")) {

                for (i = 0; i < 6; i++) {
                    arpinf->ip_mac[i] = *(unsigned char *)(pkt_data + 22 + i);

                }
                qDebug()<<"获取自己主机的MAC地址成功!thread";

            }
        }else if(res==0){
            if(sendflag){

            }
            if (pcap_sendpacket(arpinf->adhandle, sendbuf, 42) == 0) {
                sendflag = true;
                qDebug()<<"PacketSend succeed  checknet";
            }
            else {
                qDebug()<<"PacketSendPacket in getmine Error: "<<GetLastError();
            }
        }
    }

    //this->arpinf->upmactab();             //此语句会致使线程之间出现问题。现象是，网络连接检测线程出现阻塞

}


void arpgetthread::sendallarp()
{
    qDebug()<<"发送探测mac线程stop";
    sendthreadstopped = true;
}

void arpgetthread::sendmacarpbegin()
{
    sendthreadbegin = true;
}

QString arpgetthread::mactoqstring(char *mac)
{
    return QString("%1%2%3%4%5%6").arg(mac[0]).arg(mac[1]).arg(mac[2]).arg(mac[3]).arg(mac[4]).arg(mac[5]);
}
