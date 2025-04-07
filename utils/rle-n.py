import ctypes
 
#输出1字节，其中高2位为标记，低6位为匹配数
 
class RLE():
    def __init__(self):
        self.threshold = 3  #启动压缩的阈值，大于这个阈值才有必要压缩
        self.matchBitsToSign_T = {8:3, 6:2, 5:1}  #匹配数到标记的映射
        self.matchOrder = [8, 6, 5]  #设置匹配顺序，即优先匹配多少字节
 
    #比较两个字节中从高位开始的比特位匹配数，返回标记号
    def cmpSameBits(self, byte1, byte2):
        for matchBits in self.matchOrder:
            if (byte1 >> (8 - matchBits)) == (byte2 >> (8 - matchBits)):
                return self.matchBitsToSign_T[matchBits]
        return 0
 
    #文件压缩
    def RLE_encode(self, readfilename, writefilename):
        fread = open(readfilename, "rb")
        fwrite = open(writefilename, "wb")
        buf = b''   #前向缓冲区
        bufSize = 63 + self.threshold  #前向缓冲区大小
        singleStore = b''  #无匹配的数据暂存区
        singleStoreSize = 63  #无匹配的数据长度，没有加上阈值是因为匹配数可能小于阈值
 
        buf = fread.read(bufSize - len(buf))  #尽可能将buf填满
 
        while len(buf) >= self.threshold:
            curIndex = 0  #当前匹配位置
            cmpValue = self.cmpSameBits(buf[curIndex], buf[curIndex + 1])  #比较2个数据
            if cmpValue != 0:  #如果和下一个数据存在匹配关系
                cmpValue2 = self.cmpSameBits(buf[curIndex + 1], buf[curIndex + 2])  #再往后比较两个数据
                if cmpValue == cmpValue2:  #如果还存在相同的匹配关系
                    curIndex += 1
                    #不断向下匹配具有相同匹配关系的数据
                    while ((curIndex + 2) < (len(buf))) and (cmpValue == self.cmpSameBits(buf[curIndex + 1], buf[curIndex + 2])):
                        curIndex += 1
                    # 相同的数据搜索完毕，如果之前有无匹配的数据没写入文件，先写入
                    if singleStore != b'':
                        fwrite.write(bytes(ctypes.c_uint8(len(singleStore))))
                        fwrite.write(singleStore)
                        singleStore = b''
 
                    fwrite.write(bytes(ctypes.c_uint8((cmpValue << 6) + curIndex + 2 - self.threshold)))  #写入 标记+匹配数
 
                    bitsStore = 0  #比特位暂存区，相当于一个比特位队列
                    bitsCnt = 0  #比特位暂存区存在的比特位数目
                    for matchBits in self.matchBitsToSign_T:  #寻找是哪一种匹配类型
                        if self.matchBitsToSign_T[matchBits] == cmpValue:
                            if matchBits == 8:  #如果匹配数是8，特殊情况特殊处理
                                fwrite.write(bytes(ctypes.c_uint8(buf[0])))
                            else:  #其他匹配数的处理都是有通性的
                                fwrite.write(bytes(ctypes.c_uint8(buf[0])))
                                for num in range(1, curIndex + 2):  #将匹配的数据全部以比特位为单位进行编码
                                    bitsStore += (((buf[num] << matchBits) & 0xFF) >> matchBits) << bitsCnt #处理完的比特位进入队列
                                    bitsCnt += (8 - matchBits)
                                    if bitsCnt >= 8:  #队列中比特位数大于等于8了即一个字节，就可以输出一次到文件了
                                        fwrite.write(bytes(ctypes.c_uint8(bitsStore & 0xFF)))
                                        bitsStore >>= 8  #出队列
                                        bitsCnt -= 8
                            break
 
                    if bitsCnt > 0:  #将队列中剩余的数据写入文件
                        fwrite.write(bytes(ctypes.c_uint8(bitsStore & 0xFF)))
                else:
                    singleStore += buf[curIndex: curIndex + 2]
 
                buf = buf[curIndex + 2 : ] #从buf里清掉已经处理过的数据
            else:
                singleStore += buf[curIndex: curIndex + 1]  #将1个不能匹配的数据加入暂存区
                buf = buf[curIndex + 1:]  #从buf里清掉已经处理过的数据
 
            # 无匹配的数据存满了暂存区需要写入一次文件
            if len(singleStore) >= singleStoreSize:
                fwrite.write(bytes(ctypes.c_uint8(singleStoreSize)))
                fwrite.write(singleStore[0:singleStoreSize])
                singleStore = singleStore[singleStoreSize:]
 
            buf += fread.read(bufSize - len(buf))  #尽可能将buf填满
 
        singleStore += buf
 
        # 无匹配的数据存满了暂存区需要写入一次文件
        if len(singleStore) >= singleStoreSize:
            fwrite.write(bytes(ctypes.c_uint8(singleStoreSize)))
            fwrite.write(singleStore[0:singleStoreSize])
            singleStore = singleStore[singleStoreSize:]
 
        if singleStore != b'':
            fwrite.write(bytes(ctypes.c_uint8(len(singleStore))))
            fwrite.write(singleStore)
 
        fread.close()
        fwrite.close()
 
    #文件解压
    def RLE_decode(self, readfilename, writefilename):
        fread = open(readfilename, "rb")
        fwrite = open(writefilename, "wb")
 
        sign = fread.read(1)  #读取标记字节
        while sign != b'':
            if (sign[0] >> 6) == 0:  #直接输出原始数据
                buf = fread.read(sign[0])
                fwrite.write(buf)
            else:
                for matchBits in self.matchBitsToSign_T: #寻找对应的匹配类型
                    if self.matchBitsToSign_T[matchBits] == (sign[0] >> 6):
                        if matchBits == 8:  #特殊情况特殊处理
                            fwrite.write(fread.read(1) * ((sign[0] & 0x3F) + self.threshold))
                        else:
                            num = (sign[0] & 0x3F) + self.threshold  #需要解压的数据个数
                            same = fread.read(1)  #取一个模板数据
                            fwrite.write(same)
                            bitsStore = 0  #比特位队列
                            bitsCnt = 0  #记录比特位队列中比特位的数目
                            for i in range(1, num):  #不断从比特位队列中解压出数据
                                if bitsCnt < (8 - matchBits):
                                    tmp = fread.read(1)
                                    if tmp == b'':
                                        break
                                    bitsStore += tmp[0] << bitsCnt
                                    bitsCnt += 8
 
                                fwrite.write(bytes(ctypes.c_uint8(((same[0] >> (8 - matchBits)) << (8 - matchBits)) + (((bitsStore << matchBits) & 0xFF) >> matchBits))))
                                bitsStore >>= (8 - matchBits)
                                bitsCnt -= (8 - matchBits)
                        break
 
            sign = fread.read(1)  #读取下一个标记字节
 
        fread.close()
        fwrite.close()
 
if __name__ == '__main__':
    Demo = RLE()
    Demo.RLE_encode("../../CH582_WAV/EB-ALOW_8bit.wav", "../../CH582_WAV/EB-ALOW_8bit.rle-n")
