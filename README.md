<!--
 * @Author: lizhiyuan
 * @Date: 2019-10-06 15:06:49
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-03-19 10:02:56
-->
# libuv源码分析

## 1. socket阻塞模式和非阻塞模式

网络传输数据链路分析

```js
客户端A ------> write(进程被阻塞) -----> sys_write(系统调用,软中断) ------> 数据写入内核缓冲区(写缓冲区) -----> 网卡封包 ------->  路由器/交换机 ------->  服务器网卡发现数据到来产生硬中断(CPU进行系统调用,切换到内核态)-----> 内核放入该套接字的读缓冲区 ----> epoll_wait唤醒套接字进程
```

![](./image/socket.png)

- connect 客户端连接的时候默认也是阻塞的
- accept 服务器接收连接的时候默认也是阻塞的
- send / write 发送数据是阻塞的
- recv / read 读取数据也是阻塞的

所谓的阻塞模式,就是当某个函数`执行成功的条件`当前不能满足的时候,该函数会阻塞当前执行的进程,感觉就是IO默认都是阻塞的...

而非阻塞即使某个函数的执行条件不能满足，该函数也不会阻塞当前的执行，而是立即返回.

![](./image/block.png)

send函数的本质并不是直接往网络上发送数据,而是将应用层发送缓冲区的数据拷贝到内核缓冲区(网卡缓冲区)

recv函数本质上也并不是从网络上收取数据，而只是将内核缓冲区中的数据拷贝到应用程序的缓冲区中

不同的程序进行网络通信的时候,发送的一方会将内核缓冲区的数据通过网络传输给接收方的内核缓冲区。假设应用程序A不断的调用send函数,这样数据会不断拷贝到对应的内核缓冲区，如果B端迟迟不调用recv函数,那么B的内核缓冲区被填满后,A的内核缓冲区也会被填满

1. 当socket是阻塞模式的的话，继续调用send/recv会导致程序阻塞,发的太快,接的很慢;发的很慢,接的很快都会阻塞

```
其实这个内核缓冲区就是TCP协议中的窗口概念
建立连接的时候，三次握手除了沟通序号之外,还要沟通窗口大小

// block_client 发出SYN 标记位, 发出自己的窗口大小 ,以及自己的序号 
11:52:35.907381 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [S], seq 1394135076, win 43690, options [mss 65495,sackOK,TS val 78907688 ecr 0,nop,wscale 7], length 0

// block_server ACK + SYN   确认客户端的序号+1 ,发出自己的窗口大小以及自己的序号
20:32:21.261484 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [S.], seq 1233000591, ack 1394135077, win 43690, options [mss 65495,sackOK,TS val 78907688 ecr 78907688,nop,wscale 7], length 0

// block_client ACK 确认服务器的序号 + 1  ,窗口大小从一个比较小的数值开始,告知服务器我的接收能力
11:52:35.907441 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0

// 开始正式传输数据,客户端发送10个字节给服务器,077-087 
11:52:35.907615 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135077:1394135087, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10

// block_server 确认087已经发送成功
11:52:35.907626 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135087, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0

11:52:35.907785 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135087:1394135097, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907793 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135097, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907809 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135097:1394135107, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907814 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135107, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907839 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135117:1394135127, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907853 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135127:1394135137, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907880 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135147:1394135157, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907896 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135167, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907920 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [P.], seq 1394135177:1394135187, ack 1233000592, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 10
11:52:35.907924 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135187, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.907938 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394135197, win 342, options [nop,nop,TS val 78907688 ecr 78907688], length 0
11:52:35.923799 IP 127.0.0.1.40846 > 127.0.0.1.3000: Flags [.], seq 1394135247:1394157135, ack 1233000592, win 342, options [nop,nop,TS val 78907704 ecr 78907688], length 21888
11:52:35.923840 IP 127.0.0.1.3000 > 127.0.0.1.40846: Flags [.], ack 1394157135, win 1365, options [nop,nop,TS val 78907704 ecr 78907688], length 0
```

可以知道的是block_server的窗口短期内慢慢增加，后面随着缓冲区数据积压越来越多，TCP的窗口会慢慢变小，最终为0

然后再发送的时候就会把自己的缓冲区也填满了

2. 当socket是非阻塞的时候,数据发不出去了，但是 send 函数不会阻塞，而是立即返回，返回值是 -1（Windows 系统上 返回 SOCKET_ERROR，这个宏的值也是 -1），此时得到错误码是 EWOULDBLOCK

3. 当socket是阻塞模式的时候,程序阻塞在recv函数中

4. 当socket是非阻塞模式的时候,如果当前无数据可读，recv 函数将立即返回，返回值为 -1，错误码为 EWOULDBLOCK


> 其实一般情况下,无特殊需求,客户端的请求都是阻塞模式的,加快的方式只是采用了异步的形式,不需要非常显示的设置客户端的非阻塞模式



***总结一下:*** 

###  TCP发送方发送大量数据的处理方法(因为涉及到网络封包)

```
// send大量的数据
char szSendBuf[1024 * 64 * 10] = {0};
int iRet = send(sockClient, szSendBuf, sizeof(szSendBuf) , 0);

// 实际是没有任何问题的,TCP内核会将这些大量的数据拆分成多个包同时发送出去
// 但是读的时候就要while循环的将这些数据批量接收
while(1)
	{
	    char szRecvBuf[1024 * 64] = {0};
	    int iRet = recv(sockConn, szRecvBuf, sizeof(szRecvBuf), 0);
		total += iRet;
	    printf("iRet is %u, total %u\n", iRet, total);
		getchar(); 
	}
// 发送的时候发送一次就可以了
// 接收的时候需要多次的接收
// 底层的TCP内核程序会将这些数据拆成包发送....
// TCP因为会拆包所以，数据的边界是不清晰的，需要应用层的协议去定义如何去拆包    
```


###  TCP发送方发送过小数据的处理方法(因为涉及到网络封包)

TCP使用以下条件来决定何时在收到的数据包上发送ACK

1. 如果在延迟(200ms)计时器过期之前收到了第二个数据包,则发送ACK
2. 如果在收到第二个数据包且延迟计时器过期之前,有于ACK相同方向发送的数据,则会将该ACK与数据段合并立即发送
3. 当延迟计时器过期的时候,发送ACK

可以看到,无论任何时候,ACK的包都需要及时的发送,发送是为了保证TCP的包的可靠性,需要接收方不断的确认接收到的数据...

在此基础上,为了避免不断的发送ACK,启动了延迟ACK

**所以,当客户端发来数据的时候,服务器并没有立即ACK,而是等待了一段时间,等待的这段时间就是看服务器是否也会发送数据给客户端,将ACK携带着给客户端提高利用率**

**所以如果数据是单向的客户端不断的给服务器发送数据,你看到的就是在第二包发送成功之前就收到了来自服务器的ACK,所以你看到的就是a--延迟---ACK--b--延迟--ACK-c--延迟--ACK....**

这里注意一点,b包发送的时间是 延迟的时间 + 收到a的ACK的时间

这个默认是开启的状态,TCP_NODELAY为开启状态

可以选择将它关闭,关闭的情况主要考虑到是对实时性要求比较高的场景,类似用户的鼠标操作/键盘输入/状态更新这种连续的小分组数据,需要立刻在对端立即呈现.






### TCP发送方发送数据过快的处理方法(客户端发送的总是很快,但服务器接收的速度总是很慢)

对比文件,写入磁盘的速度远比读取数据慢的多了,所以,一切都是为了避免写入的数据过快来不及处理

NodeJS中为了不积压,pipe当可写流返回false的时候,可读流停止..可写流drain的时候再恢复可读流;另外hightWaterMark也定义了可读流从底层读取数据的大小,精准的控制这个大小可避免可读流读的太快...

在TCP中, 增加了窗口的概念,接收方将自己的`窗口大小`发送给发送方,发送方根据接收方的窗口大小决定发送的速度

我的理解并不是在一个ACK之前,可发送的最大的数据包,而是在未达到窗口大小前,发送放可一直处于不断发送阶段,发送过程不能停....当达到窗口大小为0的时候,发送过程终止....

自动调整窗口大小就是TCP`流量控制`

###  TCP发送方发送数据延时/网络情况不好

流量控制是受接收能力影响的.但拥塞问题是受网络环境影响的.这就是为了解决数据延迟的`拥塞避免`.同样都是通过调整窗口大小来避免拥塞..

当接收方的接收能力比较差的时候,即使网络非常通畅,发送方也要根据接收方的能力限制自己的发送窗口.当网络环境较差的时候,即使接收方有很强的接收能力,发送方也要根据网络的拥塞情况来限制自己的发送窗口.

所以,实际的窗口大小 = min(cwnd(拥塞窗口),rwnd(流量控制的滑动窗口))

你看到的就是窗口会根据网络的情况逐步加大,又会根据网络的情况,实时的调整....


### TCP丢失、重复、失序情况

如何保证可靠的传输呢,主要是这么几个:

- 序号和确认号,A在发送的数据包中增加一个序号(seq),同时b要在ACK包增加一个确认号(ack)

- 基于时间,设置一个计时器来判断数据传输是否超时(超时重传)
- 基于确认信息(快速重传 + SACK)




### 总结整理

我们先来复习一下阻塞模式和非阻塞模式

在阻塞模式下,服务器承载多个请求,如果每个请求来了开辟一个线程,或者开辟一个进程是不合理的

那么,我们自然就想到,如何才能让网络传输变成不阻塞的状态, 首先要在客户端保证请求不被阻塞

***客户端请求不阻塞***

所有的IO操作都会大大降低程序的运行速度,所以,可以将所有的IO操作放入EventLoop中，当条件达到的时候触发回调函数执行.

正常的程序的临时变量、函数的参数都会入栈,函数调用的地址也会入栈,对于异步任务的回调函数,它是暂时不入栈执行的,当eventLoop中触发后,再放入栈中.

所以,总的来看,实现一个eventLoop的关键其实就是创建一个线程,主线程负责执行同步的任务,其他的线程用来执行异步的任务,当有异步的定时任务或者事件任务发生的时候,通知主线程执行就行了

客户端发可以通过浏览器、操作系统(socket)创建socket对象

这段程序是无法做到不阻塞主进程的,所以,应有单独独立的线程工作,或者排队处理

浏览器的eventLoop线程会将异步任务的代码扔到队列去,按照微任务--->宏任务的执行顺序依次循环往复调用,一旦有成功的任务,再扔到主进程中执行


```
// 客户端使用while的主要情况是为了保持跟服务器的长连接,可以在适当的时候跳出while
// 也可以直接write / read完成任务后直接退出
while(1){
	// 将客户端的数据写给服务器
	write()
	// 获取服务器的数据,并处理
	read()
}
// 处理完毕后关闭连接
close()
// 程序结束
return 0
```

```
// `eventLoop` is an array that acts as a queue (first-in, first-out)
var eventLoop = [ ];
var event;

// keep going "forever"
while (true) {
    // perform a "tick"
    if (eventLoop.length > 0) {
        // get the next event in the queue
        event = eventLoop.shift();

        // now, execute the next event
        try {
            event();
        }
        catch (err) {
            reportError(err);
        }
    }
}
```

***服务器响应不阻塞(这里的不阻塞其实指的是不阻塞其他的请求)***

服务器需要并行处理多个请求,这对多线程/多进程的服务器的压力非常的大,服务器的资源毕竟有限,来个请求一个进程/线程开销过大

所以,服务器高性能解决多个请求的核心在于epoll

我们可以把所有的请求看做是一个文件句柄,当客户端没有请求过来的时候,服务器阻塞监听,当客户端的请求到来的时候,首先进行三次握手,建立连接后,加入到监听的数组中.

当请求再次到来的时候,我们会有一个红黑树的epoll,我们为所有客户端的请求都建立了一个文件句柄,请求来的时候,意味着文件句柄的读事件触发了,epoll会遍历所有触发的句柄找到对应的文件句柄,然后我们就可以去进行代码调用了

```
edf = epoll_create // 创建我们的epoll

// 你可以理解为我把服务器创建的socket文件句柄放到监听数组中去了...
// 如果有请求过来的时候,会触发服务器的读事件
res = epoll_ctl(efd,EPOLL_CTL_ADD,listenfd,&tep)
while(1){
	nready = epoll_wait(efd,ep,OPEN_MAX,-1) // 这里会阻塞监听数组的变化,并返回数组
    for(i=0;i<nready;i++){
        if(ep[i].data.fd == listenfd){
            // 处理服务器的读事件,三次握手
            connfd = Accpet()
            // 将事件添加到监听数组中去
            res = epoll_ctl()
        }else{
            // 处理客户端的读事件
            sockfd = ep[i].data.fd
            // write给客户端发送数据
        }
    }
}
```


### redis异步的实现原理

这里,其实是把异步的任务扔给了libuv让他去执行了,而libuv在浏览器或者nodeJS看来就是一个独立的处理异步任务的线程

深入到线程内部,实际就是一个存储异步任务的队列,拿到队列中的任务一个个的调用,其实也是阻塞排队的,只不过不影响主进程了而已...所以,假设任务较多的话,也会慢的....

所以,我们了解libuv的目的就很简单了,看一下这个eventLoop是如何存储任务的,然后是如何调用任务的..


如何处理一些定时的任务,就可以了... 我理解,把任务扔到libuv里面去,应该非常的简单,任务执行完毕后如何通知主进程任务done,就基本完成了整个线索

我们就看redis是如何实现的异步的库

1. 首先先看一下我们的各系统的高并发的实现,mac下是使用kqueue,unix系统用的是epoll

```
// 使用aeApiCreate方法创建epoll实例
static int aeApiCreate(aeEventLoop *eventLoop){
    
}

```
































