import ctypes
 
class RLE():
    def __init__(self):
        self.threshold = 3  #启动压缩的阈值，大于这个阈值才有必要压缩
 
    #文件压缩
    def RLE_encode(self, readfilename, writefilename):
        fread = open(readfilename, "rb")  #以二进制方式读取文件
        fwrite = open(writefilename, "wb")  #以二进制方式写入文件
        buf = b''  #前向缓冲区
        bufSize = 127 + self.threshold  #前向缓冲区大小
        singleStore = b''  #无匹配的数据暂存区
        sigleStoreSize = 127  #无匹配的数据长度，没有加上阈值是因为匹配数可能小于阈值
 
        buf = fread.read(bufSize - len(buf))  #尽可能将buf填满
 
        while len(buf) >= self.threshold:  #大于3个字节才有必要压缩
            curIndex = 0  #当前匹配位置
            if (buf[curIndex] == buf[curIndex + 1]):  #如果和下一个数据相同
                if (buf[curIndex + 1] == buf[curIndex + 2]):  #如果往后两个数据都是相同的，启动压缩
                    curIndex += 1
                    #不断向后寻找相同的数据
                    while ((curIndex + 2) < (len(buf))) and (buf[curIndex + 1] == buf[curIndex + 2]):
                        curIndex += 1
                    #相同的数据搜索完毕，如果之前有无匹配的数据没写入文件，先写入
                    if singleStore != b'':
                        fwrite.write(bytes(ctypes.c_uint8(len(singleStore))))  #写入 标记+匹配数
                        fwrite.write(singleStore)  #写入所有无匹配的数据
                        singleStore = b''  #清空暂存区
                    fwrite.write(bytes(ctypes.c_uint8((1 << 7) + curIndex + 2 - self.threshold)))  #写入 标记+匹配数
                    fwrite.write(bytes(ctypes.c_uint8(buf[0])))  #写入这个重复出现的数据
                else:
                    singleStore += buf[curIndex: curIndex + 2]  #将2个不能匹配的数据加入暂存区
 
                buf = buf[curIndex + 2 : ]  #从buf里清掉已经处理过的数据
            else:
                singleStore += buf[curIndex: curIndex + 1]  #将1个不能匹配的数据加入暂存区
                buf = buf[curIndex + 1:]  #从buf里清掉已经处理过的数据
 
            # 无匹配的数据存满了暂存区需要写入一次文件
            if len(singleStore) >= sigleStoreSize:
                fwrite.write(bytes(ctypes.c_uint8(sigleStoreSize)))  # 写入 标记+匹配数
                fwrite.write(singleStore[0:sigleStoreSize])  # 写入bufSize个无匹配的数据
                singleStore = singleStore[sigleStoreSize:]  # 从暂存区里清掉已经处理过的数据
 
            buf += fread.read(bufSize - len(buf))  #尽可能将buf填满
 
        singleStore += buf  #将前向缓冲区里剩余的数据加入无匹配数据暂存区
 
        if len(singleStore) >= sigleStoreSize:
            fwrite.write(bytes(ctypes.c_uint8(sigleStoreSize)))  # 写入 标记+匹配数
            fwrite.write(singleStore[0:sigleStoreSize])  # 写入bufSize个无匹配的数据
            singleStore = singleStore[sigleStoreSize:]  # 从暂存区里清掉已经处理过的数据
 
        if singleStore != b'':
            fwrite.write(bytes(ctypes.c_uint8(len(singleStore))))
            fwrite.write(singleStore)
 
        fread.close()
        fwrite.close()
 
    #文件解压
    def RLE_decode(self, readfilename, writefilename):
        fread = open(readfilename, "rb")  #以二进制方式读取文件
        fwrite = open(writefilename, "wb")  #以二进制方式写入文件
 
        sign = fread.read(1)  #读取标记字节
        while sign != b'':
            if sign[0] >= (1<<7):  #如果是压缩标记
                fwrite.write(fread.read(1) * (sign[0] - (1 << 7) + self.threshold))  #解压释放数据
            else:  #如果不是压缩标记
                buf = fread.read(sign[0])  #读取无匹配的所有数据
                fwrite.write(buf)  #写入无匹配的数据
 
            sign = fread.read(1)  #读取下一个标记字节
 
        fread.close()
        fwrite.close()
 
 
if __name__ == '__main__':
    Demo = RLE()
    Demo.RLE_encode("../../CH582_WAV/EB-ALOW_8bit.wav", "../../CH582_WAV/EB-ALOW_8bit.rle-o1")

    