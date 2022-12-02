# -*- coding: utf-8 -*-

import os
import sys

class CHandle:
    def __init__(self, sType, sRootPath):
        self.m_Type = sType
        self.m_RootPath = sRootPath
        self.m_Path = "lua\%s\%s_scene\mapparts"%(sType, 20)
        self.m_FilePath = os.path.join(self.m_RootPath, self.m_Path)
        self.m_Checked = 0
        self.CheckPath()

    def CheckPath(self):
        if not os.path.exists(self.m_FilePath):
            print("please check your input path!")
            input('Press Enter to exit...')
            sys.exit()
        self.m_Checked = 1
    
    def Handle(self):
        if not self.m_Checked:
            return
        print("process %s ...."%(self.m_Type))
        os.chdir(self.m_FilePath)
        lstFile = os.listdir()
        for sFile in lstFile:
            with open(sFile, 'r+') as f:
                lstContent = f.readlines()
                for index, sContent in enumerate(lstContent):
                    if sContent.find("playerBornList")!= -1:
                        lstVal = sContent.split("=")
                        sTable = lstVal[1]
                        sTable1 = sTable.replace("{", "[")
                        sTable2 = sTable1.replace("}", "]")
                        try:
                            lstTable = eval(sTable2)
                        except:
                            print("%s %s ignore "%(sFile, sTable))
                            continue
                        lstTable.sort()
                        sResult = str(lstTable)
                        sResult1 = sResult.replace("[", "{")
                        sResult2 = sResult1.replace("]", "}")
                        sResult3 = sResult2.replace(" ", "")
                        lstVal[1] = " " + sResult3
                        sFill = "=".join(lstVal)
                        lstContent[index] = sFill + "\n"
                        f.seek(0)
                        f.truncate()
                        f.writelines(lstContent)
        print("process %s finish"%(self.m_Type))


if __name__ == "__main__":
    sRootPath = input("please input qz_config path: \n")
    # sRootPath = "D:\qz_Project\qz_config"
    oClientHandle = CHandle("client", sRootPath)
    oClientHandle.Handle()
    oServerHandle = CHandle("server", sRootPath)
    oServerHandle.Handle()
    print("handle finish !!!")
    input('Press Enter to exit...')