#!/usr/bin/python3
# -*- coding: utf-8 -*-

import os
import sys
import time
import inspect
import traceback
import json

# --------------------------------------------------
# parameter
# --------------------------------------------------
G_VERSION = '0.0.1'


# --------------------------------------------------
# dora
# --------------------------------------------------

class DORA:
    '''
    说明
        dragon evb/fpga硬件调试平台
    举例
        dora = DORA()
    '''

    def __init__(self, v_boardInfo='',
                 v_doraRoot=None,
                 v_mpLogQueue=None, v_log=True, v_dbg=False, v_ldbg=True,
                 v_chanShow=False):
        '''
        初始化方法

        参数
            v_boardInfo  - 可选, 默认 ''        指定board info的json文件, 参考config/board_info_example.json
                                                不指定时, 自动扫描board info
            v_doraRoot   - 可选, 默认 None      ww_dora的根目录, 适用于把dora.py挪到其他位置时使用
            v_mpLogQueue - 保留, 默认 None
            v_log        - 可选, 默认 True      初始化时, dora.py的日志开关
            v_dbg        - 可选, 默认 False     初始化时, dora.py的调试开关
            v_ldbg       - 可选, 默认 True      初始化时, dora.py调用的底层模块的调试开关
            v_chanShow   - 可选, 默认 False     初始化前, 显示ftdi channel信息
        举例
            dora = DORA(v_ldbg=False, v_chanShow=False)
        '''

        self.m_board  = None
        self.m_jtag   = None
        self.m_config = None
        self.m_i2c    = None

        # multi process queue, 用于gui
        self.mpLogQueue = None

        # dora根目录
        self.doraRoot = ''

        # board的详细信息
        self.boardsInfo = []
        self.boardsStus = {}
        self.boardsType = {}
        self.boardsJson = {}

        # 用户board信息json文件
        self.boardsInfoJson = v_boardInfo

        # channel的handle
        self.boardsHandle = {}

        # 调试开关
        self.dbg = v_dbg
        self.log = v_log

        # 颜色
        self.YELLOW = '\033[93m'
        self.RED    = '\033[91m'
        self.RESET  = '\033[0m'

        #常量
        self.SMBUS_ADDR_MASK = 0xe000
        self.BOOT_MODE_MASK = 0x0048

        # 回车换行
        self.end = os.linesep

        # 模块加载
        self.__api_import(v_doraRoot)

        # 设置low level调试开关
        self.m_board.board_debug_set(v_ldbg)

        # 设置libmpss调试等级
        self.m_jtag.ftdi_debug_level_set(2) # less log
        # self.m_jtag.ftdi_debug_level_set(7) # more log

        if v_chanShow:
            self.f_hardware_chan_show()

        # 初始化
        self.m_jtag.jtag_reg_lock_set(False)
        self.__board_info_init()
        self.__board_stus_init()
        self.__board_type_determine()

        for boardId in self.boardsHandle.keys():
            self.__board_handle_close(boardId, 'I2C')

    def __del__(self):
        self.__dbg()
        self.m_jtag.jtag_reg_lock_set(True)
        self.m_jtag.jtag_set_check(True)
        for boardId in self.boardsHandle.keys():
            self.__board_handle_close(boardId)

#############################################################
# baisc
#############################################################
    def __trace(self):
        stack = inspect.stack()
        if len(stack) >= 3:
            frame_info = stack[2] # upper upper caller
            return f'line:{frame_info.lineno} function:{frame_info.function}'
        else:
            return ''

    def __print(self, v_str, v_pos=False):
        if self.mpLogQueue:
            self.mpLogQueue.put((f'{v_str}', v_pos))
        else:
            if v_pos:
                print(f'\r{v_str}', end='', flush=True)
            else:
                print(f'{v_str}', end=self.end)

    def __dbg(self, v_str='', v_pos=False):
        if not self.dbg:
            return
        self.__print(f'\r[dora]-[dbg] {self.__trace()} {v_str}', False)

    def __log(self, v_str, v_pos=False):
        if not self.log:
            return
        self.__print(f'\r[dora]-[log] {v_str}', v_pos)

    def __warn(self, v_str):
        self.__print(f'\r{self.RED}[dora]-[warn] {v_str}{self.RESET}', False)

    def __raise(self, v_str):
        self.__warn(v_str)
        # raise RuntimeError(v_str) from None
        raise RuntimeError('')

    def __except_traceback(self):
        return f'{self.YELLOW}\r\n[dora]\r\n{traceback.format_exc()}{self.RESET}'

    def __check_if_bin_valid(self, v_binFile):
        with open(v_binFile, 'rb') as f:
            binFileTxt = f.read()
            if 'wdr1108'.encode('utf-8') not in binFileTxt:
                self.__raise(f'invalid bin file:{v_binFile}')

#############################################################
# import
#############################################################
    def __api_import(self, v_doraRoot=None):
        if v_doraRoot:
            doraRoot = os.path.abspath(v_doraRoot)
        else:
            doraRoot = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

        # 检查dora根目录
        if not os.path.exists(os.path.join(doraRoot, 'api/config.py')) and \
           not os.path.exists(os.path.join(doraRoot, 'api/config.pyc')):
            raise RuntimeError(f'dora root dir error -> please specify by v_doraRoot=')

        # 添加dora根目录到sys.path, 以便import
        if doraRoot not in sys.path:
            sys.path.insert(0, doraRoot)

        # 导入dora api
        if not self.m_board:
            import api.board as board
            self.m_board = board

        if not self.m_jtag:
            import api.jtag as jtag
            self.m_jtag = jtag

        if not self.m_config:
            import api.config as config
            self.m_config = config

        if not self.m_i2c:
            import api.i2c as i2c
            self.m_i2c = i2c

        self.doraRoot = doraRoot

#############################################################
# handle
#############################################################
    def __board_handle_get_jtag(self, v_boardId):
        self.__dbg()

        function = 'JTAG'

        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid'] # == v_boardId

        if self.boardsHandle[boardId][function]:
            return self.boardsHandle[boardId][function]

        for chanInfo in boardInfo['channels']:
            if chanInfo['function'] != function:
                continue

            chanid = chanInfo.get('chanid', None)
            chanSn = chanInfo.get('sn', None)
            chanLocid = chanInfo.get('locid', None)

            if chanSn == '' and chanLocid == 0:
                self.__raise(f'board {boardId} jtag channel sn and locid empty')

            while True:
                # if chanid != None:
                #     try:
                #         self.__dbg(f'board {boardId} open jtag channel by chanid -> chanid:{chanid}')
                #         self.boardsHandle[boardId][function] = self.m_jtag.jtag_open_channel(chanid, 'idx')
                #         break
                #     except:
                #         pass

                if chanLocid != None:
                    try:
                        self.__dbg(f'board {boardId} open jtag channel by locid -> locid:{chanLocid}')
                        self.boardsHandle[boardId][function] = self.m_jtag.jtag_open_channel(chanLocid, 'loc')
                        break
                    except:
                        pass

                if chanSn != None:
                    try:
                        self.__dbg(f'board {boardId} open jtag channel by sn -> sn:{chanSn}')
                        self.boardsHandle[boardId][function] = self.m_jtag.jtag_open_channel(chanSn, 'sn')
                        break
                    except:
                        pass

                self.__raise(f'board {boardId} open jtag channel fail')

            clock = chanInfo.get('clock_rate', 1000000)

            try:
                self.__dbg(f'board {boardId} init jtag channel -> function:{function} clock:{clock} {self.boardsHandle[boardId][function]}')

                if self.boardsStus[boardId] == True:
                    self.m_jtag.jtag_init_channel_new(self.boardsHandle[boardId][function], clock)
                    self.m_jtag.jtag_init(self.boardsHandle[boardId][function])
                else:
                    self.m_jtag.ftdi_init_channel(self.boardsHandle[boardId][function], clock)
                    self.m_jtag.jtag_init(self.boardsHandle[boardId][function])
            except:
                self.__raise(f'board {boardId} init jtag channel fail')

            return self.boardsHandle[v_boardId][function]

        self.__raise(f'board {boardId} get handle fail -> function:{function}')

    def __board_handle_get_i2c(self, v_boardId):
        self.__dbg()

        function = 'I2C'

        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid'] # == v_boardId

        if self.boardsHandle[boardId][function]:
            return self.boardsHandle[boardId][function]

        for chanInfo in boardInfo['channels']:
            if chanInfo['function'] != function:
                continue

            chanid = chanInfo.get('chanid', None)
            chanSn = chanInfo.get('sn', None)
            chanLocid = chanInfo.get('locid', None)

            if chanSn == '' and chanLocid == 0:
                self.__raise(f'board {boardId} i2c channel sn and locid empty')

            while True:
                # if chanid != None:
                #     try:
                #         self.__dbg(f'board {boardId} open i2c channel by chanid -> chanid:{chanid}')
                #         self.boardsHandle[boardId][function] = self.m_i2c.i2c_open_channel(chanid, 'idx')
                #         break
                #     except:
                #         pass

                if chanLocid != None:
                    try:
                        self.__dbg(f'board {boardId} open i2c channel by locid -> locid:{chanLocid}')
                        self.boardsHandle[boardId][function] = self.m_i2c.i2c_open_channel(chanLocid, 'loc')
                        break
                    except:
                        pass

                if chanSn != None:
                    try:
                        self.__dbg(f'board {boardId} open i2c channel by sn -> sn:{chanSn}')
                        self.boardsHandle[boardId][function] = self.m_i2c.i2c_open_channel(chanSn, 'sn')
                        break
                    except:
                        pass

                self.__raise(f'board {boardId} open i2c channel fail')

            clock = chanInfo.get('clock_rate', 1000000)

            try:
                self.__dbg(f'board {boardId} init i2c channel -> function:{function} clock:{clock} {self.boardsHandle[boardId][function]}')
                self.m_i2c.i2c_init_channel_new(self.boardsHandle[boardId][function], clock)
            except:
                self.__raise(f'board {boardId} init i2c channel fail')

            return self.boardsHandle[v_boardId][function]

        self.__raise(f'board {boardId} get handle fail -> function:{function}')

    def __board_handle_close(self, v_boardId, v_function='all'):
        self.__dbg(f'{v_function}')

        boardHandleInfo = self.boardsHandle[v_boardId]
        for function, handle in boardHandleInfo.items():
            if handle == None:
                continue

            try:
                if function == 'JTAG' and (v_function == 'all' or function == v_function):
                    self.__dbg(f'board {v_boardId} close channel -> function:{function} handle:{handle}')
                    self.m_jtag.jtag_close_channel(handle)
                    boardHandleInfo[function] = None
                if function == 'I2C' and (v_function == 'all' or function == v_function):
                    self.__dbg(f'board {v_boardId} close channel -> function:{function} handle:{handle}')
                    self.m_i2c.i2c_close_channel(handle)
                    boardHandleInfo[function] = None
            except:
                self.__warn(f'board {v_boardId} close channel fail ->=function:{function} handle:{handle}')


#############################################################
# board detail
#############################################################
    def __channel_para_deft(self, v_chanInfo):
        if v_chanInfo['function'] == 'JTAG':
            v_chanInfo['clock_rate'] = 1000000    # 1m
        if v_chanInfo['function'] == 'I2C':
            v_chanInfo['clock_rate'] = 400000     # 400k
        v_chanInfo['latency_timer'] = 2

    def __board_info_init_auto(self):
        self.__dbg()

        self.boardsInfo = []

        chanCnt = self.m_i2c.i2c_get_num_channels()
        self.__log(f'found {chanCnt} channel')

        chanInfoList = []
        for id in range(chanCnt):
            locid, serial, desc = self.m_i2c.i2c_get_channel_info_with_description(id)
            chanInfoList.append([id, locid, serial, desc])

        chanInfoList = sorted(chanInfoList, key=lambda x: x[1])

        boardId = 0
        boardIdCurr = None
        jtagLocIdCurr = -2
        for chanId, locid, serial, desc in chanInfoList:
            func = ''
            if desc.strip().endswith('A'):
                func = 'JTAG'
                boardIdCurr = boardId
                boardId += 1
                jtagLocIdCurr = locid
                self.boardsInfo.append({'boardid':boardIdCurr, 'channels':[]})
            elif desc.strip().endswith('B'):
                func = 'I2C'
                if locid != jtagLocIdCurr + 1:  #i2c and jtag not the same ftdi
                    continue
            else:
                self.__raise(f'chan {chanId} desc not end with A or B')

            chanInfo = {'chanid':chanId, 'locid':locid, 'sn':serial, 'function':func}
            self.__channel_para_deft(chanInfo)
            self.boardsInfo[boardIdCurr]['channels'].append(chanInfo)

        self.__log(f'found {len(self.boardsInfo)} board')

    def __board_info_init_manual(self):
        self.__dbg()
        try:
            with open(self.boardsInfoJson, 'r', encoding='utf-8') as f:
                self.boardsInfo = json.load(f)
        except:
            self.__raise(f'load board info json fail -> {self.boardsInfoJson}')

        boardIdList = [boardInfo['boardid'] for boardInfo in self.boardsInfo]
        if len(boardIdList) != len(set(boardIdList)):
            self.__raise(f'board info duplicate boardid -> {self.boardsInfoJson}')

        self.__log(f'found {len(self.boardsInfo)} board')

    def __board_info_init_post(self):
        self.__dbg()

        for boardInfo in self.boardsInfo:
            boardId = boardInfo['boardid']
            self.boardsHandle[boardId] = {}
            self.boardsStus[boardId] = False
            self.boardsType[boardId] = ''
            self.boardsJson[boardId] = ''
            for chanInfo in boardInfo['channels']:
                function = chanInfo['function']
                self.boardsHandle[boardId][function] = None

    def __board_info_init(self):
        self.__dbg()

        self.boardsHandle = {}
        self.boardsStus = {}
        self.boardsType = {}
        self.boardsJson = {}

        if self.boardsInfoJson:
            self.__board_info_init_manual()
        else:
            self.__board_info_init_auto()

        self.__board_info_init_post()

    def __board_info_get_by_id(self, v_boardId=0):
        if len(self.boardsInfo) == 0:
            self.__raise(f'no board found')

        if len(self.boardsInfo) == 1:
            return self.boardsInfo[0]

        for boardInfo in self.boardsInfo:
            if boardInfo['boardid'] == v_boardId:
                return boardInfo

        self.__raise(f'board {v_boardId} info get fail -> not found')

    def __board_info_chan_get(self, v_boardInfo, v_funciton):
        for chanInfo in v_boardInfo['channels']:
            if chanInfo['function'] == v_funciton:
                return chanInfo
        boardId = v_boardInfo['boardid']
        self.__raise(f'board {boardId} channel get fail -> {v_funciton} not found')

    def __dict_key_renew(self, v_dict, v_oldKey, v_newKey):
        value = v_dict.pop(v_oldKey)
        v_dict[v_newKey] = value

    def __board_info_adjust(self):
        if len(self.boardsInfo) == 0:
            self.__warn(f'no active board found')
            return

        if self.boardsInfoJson:
            return

        self.__dbg()

        self.boardsInfo = sorted(self.boardsInfo, key=lambda x: x['boardid'])
        for listIndex, boardInfo in enumerate(self.boardsInfo):
            boardId = boardInfo['boardid']
            if listIndex != boardId:
                boardInfo['boardid'] = listIndex
                self.__dict_key_renew(self.boardsHandle, boardId, listIndex)
                self.__dict_key_renew(self.boardsStus,   boardId, listIndex)
                self.__dict_key_renew(self.boardsType,   boardId, listIndex)
                self.__dict_key_renew(self.boardsJson,   boardId, listIndex)

    def __board_type_get(self, v_boardId):
        self.__dbg()

        boardId = v_boardId
        jtagHandle = self.__board_handle_get_jtag(boardId)

        try:
            reg1Addr = 0x4001fff8
            reg2Addr = 0x4001fffc

            try:
                self.m_jtag.jtag_write_reg(jtagHandle, reg1Addr, reg1Addr)
                self.m_jtag.jtag_write_reg(jtagHandle, reg2Addr, reg2Addr)
                regValueR = self.m_jtag.jtag_read_reg(jtagHandle, reg2Addr)
            except:
                regValueR = 0

            if regValueR == reg2Addr:
                self.__log(f'board {boardId} type -> fpga')
                self.boardsType[boardId] = 'fpga'
                self.boardsJson[boardId] = self.m_config.get_config(os.path.join(self.doraRoot, 'config', 'fpga_base.json'))
            else:
                regValueR = self.m_jtag.jtag_read_reg(jtagHandle, 0x40000000)
                if regValueR == 0x342022f3:
                    self.__log(f'board {boardId} type -> evb')
                    self.boardsType[boardId] = 'evb'
                    self.boardsJson[boardId] = self.m_config.get_config(os.path.join(self.doraRoot, 'config', 'evb_base.json'))
                else:
                    self.__log(f'board {boardId} type -> unknown')
                    self.boardsType[boardId] = 'unknown'
                    self.boardsJson[boardId] = ''
        except:
            self.__warn(f'board {boardId} type get fail')
            self.__warn(self.__except_traceback())

    def __board_type_determine(self):
        self.__dbg()
        boardIdInvalidList = []
        for index, boardInfo in enumerate(self.boardsInfo):
            boardId = boardInfo['boardid']
            if self.boardsType[boardId] == 'evb' or self.boardsType[boardId] == 'fpga':
                continue

            if self.boardsStus[boardId] == True:
                self.__board_type_get(boardId)

                if self.boardsType[boardId] != 'evb' and self.boardsType[boardId] != 'fpga':
                    boardIdInvalidList.append(boardId)
            else:
                boardIdInvalidList.append(boardId)

        # boardIdInvalidList.sort(reverse=True) # to make sure del list item work

        # for boardIdInvalid in boardIdInvalidList:
        #     boardIno = self.__board_info_get_by_id(boardIdInvalid)

        #     self.__board_handle_close(boardIdInvalid)
        #     self.boardsHandle[boardIdInvalid] = {'JTAG':None, 'I2C':None}
        #     # self.boardsStus[boardIdInvalid] = False
        #     # self.boardsType[boardIdInvalid] = ''
        #     # self.boardsJson[boardIdInvalid] = {}

        #     # del self.boardsHandle[boardIdInvalid]
        #     del self.boardsStus[boardIdInvalid]
        #     del self.boardsType[boardIdInvalid]
        #     del self.boardsJson[boardIdInvalid]

        #     self.boardsInfo.remove(boardIno)

    def __board_power_on_sub(self, v_boardId):
        self.__dbg()

        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        baseJson = self.boardsJson[boardId]

        gpioJVI, gpioIVI = self.m_config.get_gpio_state_initial(baseJson)
        gpioJVF, gpioIVF = self.m_config.get_gpio_state_final(baseJson)

        jtagHandle = self.__board_handle_get_jtag(boardId)

        gpioJVA = self.m_jtag.jtag_read_gpio_raw(jtagHandle)
        gpioJV = (gpioJVA & self.SMBUS_ADDR_MASK) | (gpioJVI & (~self.SMBUS_ADDR_MASK))
        self.m_jtag.jtag_write_gpio_raw(jtagHandle, 0xffff, gpioJV)

        try:
            i2cHandle = self.__board_handle_get_i2c(boardId)

            gpioIVA = self.m_i2c.i2c_read_gpio_raw(i2cHandle)

            gpioIV = (gpioIVA & self.BOOT_MODE_MASK) | (gpioIVI & (~self.BOOT_MODE_MASK))
            self.m_i2c.i2c_write_gpio_raw(i2cHandle, 0xffff, gpioIV)

            gpioIV = (gpioIVA & self.BOOT_MODE_MASK) | (gpioIVF & (~self.BOOT_MODE_MASK))
            self.m_i2c.i2c_write_gpio_raw(i2cHandle, 0xffff, gpioIV)

            self.__board_handle_close(boardId, 'I2C')
            i2cHandle = self.__board_handle_get_i2c(boardId)

            try:
                self.m_board.board_init_ad5593r(i2cHandle, baseJson)
                self.m_board.board_init_lmk3h0102(i2cHandle, baseJson)
                self.m_board.board_init_lmkdb1204(i2cHandle, baseJson)
            except:
                pass

            try:
                gpioIV = (gpioIVA & self.BOOT_MODE_MASK) | (gpioIVF & (~self.BOOT_MODE_MASK))
                self.m_i2c.i2c_write_gpio_raw(i2cHandle, 0xffff, gpioIV)
            except:
                pass
            self.__board_handle_close(boardId, 'I2C')
        except:
            pass

        jtagStruct = self.m_board.board_channel_struct_get_jtag(baseJson, 1000000, 2)
        self.m_jtag.jtag_init_channel(jtagHandle, jtagStruct)

        gpioJV = (gpioJVA & self.SMBUS_ADDR_MASK) | (gpioJVF & (~self.SMBUS_ADDR_MASK))
        self.m_jtag.jtag_write_gpio_raw(jtagHandle, 0xffff, gpioJV)

        self.__board_handle_close(boardId, 'JTAG')
        self.__board_handle_get_jtag(boardId)

    def __board_power_on(self, v_boardId):
        uartMgmt = self.m_board.FT232RManager()

        try:
            self.__board_power_on_sub(v_boardId)
        except:
            uartMgmt.close()
            self.__raise(self.__except_traceback())

        uartMgmt.close()

    def __board_stus_determine(self, v_boardId):
        try:
            jtagHandle = self.__board_handle_get_jtag(v_boardId)
        except:
            self.__log(f'board {v_boardId} status -> off')
            return 'off'

        try:
            regValueR = self.m_jtag.jtag_read_reg(jtagHandle, 0xf0100000)
        except:
            self.__log(f'board {v_boardId} status -> unknown 1')
            return 'unknown'

        if regValueR == 0x0200204c:
            self.__log(f'board {v_boardId} status -> on')
            self.boardsStus[v_boardId] = True
            return 'on'
        else:
            self.__log(f'board {v_boardId} status -> unknown 2')
            return 'unknown'

    def __board_stus_force_on(self, v_boardId):
        self.__dbg()

        boardId = v_boardId

        self.boardsStus[boardId] = False

        self.__log('try to boot as fpga')
        self.boardsJson[boardId] = self.m_config.get_config(os.path.join(self.doraRoot, 'config', 'fpga_base.json'))
        self.__board_power_on(boardId)
        stusStr = self.__board_stus_determine(boardId)
        if stusStr == 'on' or stusStr == 'off':
            return

        self.__log('try to boot as evb')
        self.boardsJson[boardId] = self.m_config.get_config(os.path.join(self.doraRoot, 'config', 'evb_base.json'))
        self.__board_power_on(boardId)
        self.__board_stus_determine(boardId)

    def __board_stus_init(self, v_boardId=None):
        self.__dbg()

        if v_boardId == None:
            for boardInfo in self.boardsInfo:
                boardId = boardInfo['boardid']
                self.__board_stus_init(boardId)
        else:
            boardId = v_boardId

            self.__board_handle_close(v_boardId)
            self.boardsHandle[boardId] = {'JTAG':None, 'I2C':None}
            self.boardsStus[boardId] = False
            self.boardsType[boardId] = 'unknown'
            self.boardsJson[boardId] = ''

            stusStr = self.__board_stus_determine(boardId)

    def __board_config_gpio_get(self, v_cfg, v_gpioName):
        for gpio in v_cfg.get('gpio', []):
            if gpio['name'] == v_gpioName:
                return gpio
        self.__raise(f'gpio get fail -> name {v_gpioName}')

#############################################################
# convert for f_sys_reg_byte rd/wr
#############################################################
    def __convert_to_bytes(self, v_value):
        if v_value is None:
            return b''

        if isinstance(v_value, bytes):
            return v_value
        elif isinstance(v_value, str):
            hexStr = v_value.strip()

            if not hexStr:
                raise RuntimeError('hex str empty')

            if hexStr.startswith('0x'):
                hexStr = hexStr[2:]
            if hexStr.startswith('0X'):
                hexStr = hexStr[2:]

            if len(hexStr) % 2 != 0:
                raise RuntimeError('hex str len err')
            return bytes.fromhex(hexStr)
        elif isinstance(v_value, (list, tuple)):
            return bytes(v_value)
        elif isinstance(v_value, int):
            if v_value > 255:
                raise RuntimeError(f'byte value err -> {v_value}')
            return bytes([v_value])
        else:
            raise RuntimeError(f'type not support -> {type(v_value)}')

#############################################################
# power
#############################################################
    def f_board_power_off(self, v_boardId=0):











        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']

        if self.boardsType[boardId] == 'fpga':
            self.__warn('fpga board not support power off')
            return

        gpioMask = 0xffff
        # gpioJVI, gpioIVI = self.m_config.get_gpio_state_initial(self.boardsJson[boardId])

        jtagHandle = self.__board_handle_get_jtag(boardId)
        gpioJVA = self.m_jtag.jtag_read_gpio_raw(jtagHandle)
        gpioJV = gpioJVA & self.SMBUS_ADDR_MASK
        self.m_jtag.jtag_write_gpio_raw(jtagHandle, gpioMask, gpioJV)
        self.__board_handle_close(boardId, 'JTAG')
        self.__board_handle_get_jtag(boardId)

        try:
            i2cHandle = self.__board_handle_get_i2c(boardId)
            gpioIVA = self.m_i2c.i2c_read_gpio_raw(i2cHandle)
            gpioIV = gpioIVA & self.BOOT_MODE_MASK
            self.m_i2c.i2c_write_gpio_raw(i2cHandle, gpioMask, gpioIV)
        except:
            pass
        self.__board_handle_close(boardId, 'I2C')

        self.boardsStus[boardId] = False
        self.boardsType[boardId] = 'unknown'

    def f_board_power_reset(self, v_boardId=0):











        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']

        if self.boardsType[boardId] == 'fpga':
            self.__warn('fpga board not support power reset')
            return

        self.f_board_power_off(boardId)

        self.__board_stus_force_on(boardId)

        self.__board_stus_init(boardId)

        self.__board_type_get(boardId)

        self.__board_handle_close(boardId, 'I2C')

#############################################################
# csr
#############################################################
    def f_csr_word_rd(self, v_addr, v_cnt=1, v_boardId=0):
        """
        -> 寄存器按word读

        参数
            v_addr - 必选   寄存器地址，必须4字节对齐
            v_cnt  - 可选   寄存器个数，从v_addr读取v_cnt个word

        返回
            [addr1, addr2, ... ], [value1, value2, ... ]

        例子
            dora.f_csr_word_rd(0xd0c00500)
                返回 [0xd0c00500], [0x11111111]
            dora.f_csr_word_rd(0xd0c00500, v_cnt=3)
                返回 [0xd0c00500, 0xd0c00504, 0xd0c00508], [0x11111111, 0x22222222, 0x33333333]




        """
        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        if self.boardsStus[boardId] == False:
            self.__raise(f'board status error -> board {boardId} is false')

        jtagHandle = self.__board_handle_get_jtag(boardId)

        rdAddrList = []
        rdValueList = []
        if isinstance(v_addr, int):
            if v_addr & 0x3 != 0:
                self.__raise(f'csr read addr error -> addr 0x{v_addr:08x} not word aligned')
            if v_cnt == 0:
                self.__raise(f'csr read cnt error -> cnt is 0')

            for i in range(0, v_cnt):
                value = self.m_jtag.jtag_read_reg(jtagHandle, v_addr)
                rdAddrList.append(v_addr)
                rdValueList.append(value)
                v_addr += 4
        elif isinstance(v_addr, list):
            for addr in v_addr:
                if addr & 0x3 != 0:
                    self.__raise(f'csr read addr error -> addr 0x{addr:08x} not word aligned')
                value = self.m_jtag.jtag_read_reg(jtagHandle, addr)
                rdAddrList.append(addr)
                rdValueList.append(value)
        else:
            self.__raise(f'csr read addr error -> addr type must be int or list')

        return rdAddrList, rdValueList

    def f_csr_word_wr(self, v_addr, v_value, v_boardId=0):
        '''
        -> 寄存器按word写

        参数
            v_addr  - 必选   寄存器地址，可为int或list
            v_value - 必选   寄存器值，写入寄存器地址的值，可为int或list

        例子
            dora.f_csr_word_wr(0xd0c00500, 0x11111111)
                往0xd0c00500写入0x11111111
            dora.f_csr_word_wr(0xd0c00500, [0x11111111, 0x22222222])
                往0xd0c00500写入0x11111111
                往0xd0c00504写入0x22222222
            dora.f_csr_word_wr([0xd0c00500, 0xd0c00508], [0x11111111, 0x22222222])
                往0xd0c00500写入0x11111111
                往0xd0c00508写入0x22222222







        '''
        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        if self.boardsStus[boardId] == False:
            self.__raise(f'board status error -> board {boardId} is false')

        jtagHandle = self.__board_handle_get_jtag(boardId)

        if isinstance(v_addr, int) and isinstance(v_value, int):
            self.m_jtag.jtag_write_reg(jtagHandle, v_addr, v_value)
        elif isinstance(v_addr, int) and isinstance(v_value, list):
            for i, value in enumerate(v_value):
                self.m_jtag.jtag_write_reg(jtagHandle, v_addr + i*4, value)
        elif isinstance(v_addr, list) and isinstance(v_value, list):
            if len(v_addr) != len(v_value):
                self.__raise(f'csr write error -> addr size {len(v_addr)} != value size {len(v_value)}')
            for i, addr in enumerate(v_addr):
                self.m_jtag.jtag_write_reg(jtagHandle, addr, v_value[i])
        else:
            self.__raise(f'csr write error -> addr and value type must be int or list')

    def f_csr_byte_rd(self, v_addr, v_cnt=1, v_boardId=0):
        """
        -> 寄存器按byte读

        参数
            v_addr - 必选   寄存器地址
            v_cnt  - 可选   寄存器个数，从v_addr读取v_cnt个byte

        返回
            addr, [value1, value2, ... ]

        例子
            dora.f_csr_byte_rd(0xd0c00500)
                返回 0xd0c00500, [0x11]
            dora.f_csr_byte_rd(0xd0c00501, v_cnt=4)
                返回 0xd0c00501, [0x11, 0x22, 0x33, 0x44]



        """
        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        if self.boardsStus[boardId] == False:
            self.__raise(f'board status error -> board {boardId} is false')

        jtagHandle = self.__board_handle_get_jtag(boardId)

        addr = v_addr & 0xfffffffc
        ofst = v_addr & 0x3
        cnt = ((v_cnt + ofst + 3) >> 2) << 2

        result = bytes()
        for i in range(0, cnt, 4):
            value = self.m_jtag.jtag_read_reg(jtagHandle, addr + i)
            result += value.to_bytes(4, byteorder='little', signed=False)
        result = result[ofst:v_cnt+ofst]

        return v_addr, list(result)

    def f_csr_byte_wr(self, v_addr, v_value, v_boardId=0):
        '''























        '''
        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        if self.boardsStus[boardId] == False:
            self.__raise(f'board status error -> board {boardId} is false')

        jtagHandle = self.__board_handle_get_jtag(boardId)

        byteWr = self.__convert_to_bytes(v_value)

        byteWrLen = len(byteWr)
        byteIndex = 0

        # 按字处理，减少写入次数
        while byteIndex < byteWrLen:
            # 当前字节的地址
            byteAddr = v_addr + byteIndex

            # 计算对齐的字地址和在字内的偏移
            byteAddrAlign = byteAddr & ~0x3  # 向下对齐到4字节边界
            ofstInWord = byteAddr & 0x3      # 在字内的偏移

            # 计算这个字内最多可以写入多少字节
            byteCntInWord = min(4 - ofstInWord, byteWrLen - byteIndex)

            # 读取原始字
            wordOrgin = self.m_jtag.jtag_read_reg(jtagHandle, byteAddrAlign)

            # 创建掩码，只修改目标字节
            mask = 0
            wordModify = wordOrgin

            for i in range(byteCntInWord):
                bytePos = ofstInWord + i

                byteMsk = 0xFF << (8 * bytePos)
                mask |= byteMsk

                dataByte = byteWr[byteIndex + i]

                wordModify &= ~byteMsk
                wordModify |= (dataByte << (8 * bytePos))

            self.m_jtag.jtag_write_reg(jtagHandle, byteAddrAlign, wordModify)

            byteIndex += byteCntInWord

#############################################################
# clock
#############################################################
    def f_jtag_clock_set(self, v_clock, v_boardId=0):
        '''










        '''
        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']

        chanInfo = self.__board_info_chan_get(boardInfo, 'JTAG')
        chanInfo['clock_rate'] = v_clock

        if self.boardsType[boardId] == 'fpga' and v_clock > 2000000:
            self.__warn(f'please make sure fpga hw support jtag {v_clock} rate')

        if self.boardsType[boardId] == 'evb' and v_clock > 6000000:
            self.__warn(f'please make sure evb hw support jtag {v_clock} rate')

        self.__board_handle_close(boardId, 'JTAG')
        self.__board_handle_get_jtag(boardId)

    def f_i2c_clock_set(self, v_clock, v_boardId=0):
        '''










        '''
        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']

        chanInfo = self.__board_info_chan_get(boardInfo, 'I2C')
        chanInfo['clock_rate'] = v_clock

        if self.boardsType[boardId] == 'fpga' and v_clock >= 1000000:
            self.__warn(f'please make sure fpga hw support i2c {v_clock} rate')

        if self.boardsType[boardId] == 'evb' and v_clock > 1000000:
            self.__warn(f'please make sure evb hw support jtag {v_clock} rate')

        self.__board_handle_close(boardId, 'I2C')

#############################################################
# show
#############################################################
    def f_board_show(self, v_boardId=None):
        '''











        '''
        if v_boardId == None:
            if len(self.boardsInfo) == 0:
                self.__warn('no board found')
                return

            for boardInfo in self.boardsInfo:
                boardId = boardInfo['boardid']
                self.f_board_show(boardId)
        else:
            boardInfo = self.__board_info_get_by_id(v_boardId)
            boardId = boardInfo['boardid']

            self.__log(f'==================== board {boardId} info s ====================')
            showStr  = f'{self.end}{json.dumps(boardInfo, indent=4)} {self.end}{self.end}'
            showStr += f'type   -> {self.boardsType[boardId]} {self.end}'
            showStr += f'status -> {self.boardsStus[boardId]}'
            self.__log(showStr)
            self.__log(f'-------------------- board {boardId} info e --------------------{self.end}')

    def f_ftdi_gpio_show(self, v_boardId=None):
        '''











        '''
        if v_boardId == None:
            if len(self.boardsInfo) == 0:
                self.__warn('no board found')

            for boardInfo in self.boardsInfo:
                boardId = boardInfo['boardid']
                self.f_ftdi_gpio_show(boardId)
        else:
            boardInfo = self.__board_info_get_by_id(v_boardId)
            boardId = boardInfo['boardid']

            jtagHandle = self.__board_handle_get_jtag(boardId)
            gpioJVA = self.m_jtag.jtag_read_gpio_raw(jtagHandle)

            self.__log(f'==================== board {boardId} ftdi gpio s ====================')
            showStr = f'{self.end}gpio jtag actural:0x{gpioJVA:04x} {self.end}'
            try:
                i2cHandle = self.__board_handle_get_i2c(boardId)
                gpioIVA = self.m_i2c.i2c_read_gpio_raw(i2cHandle)
                showStr += f'gpio i2c actural:0x{gpioIVA:04x}'
            except:
                pass

            self.__board_handle_close(boardId, 'I2C')

            self.__log(showStr)

            self.__log(f'-------------------- board {boardId} ftdi gpio e --------------------{self.end}')

    def f_hardware_chan_show(self):
        '''










        '''
        chanCnt = self.m_i2c.i2c_get_num_channels()
        self.__log(f'==================== ftdi channel s ====================')
        showStr = ''
        for id in range(chanCnt):
            locid, serial, description = self.m_i2c.i2c_get_channel_info_with_description(id)

            showStr += f'{self.end}chanid:{id} locid:{locid} sn:{serial} desc:{description}'
        self.__log(showStr)
        self.__log(f'-------------------- ftdi channel e --------------------{self.end}')

#############################################################
# board cpu reset
#############################################################
    def f_cpu_reset(self, v_mode, v_boardId=0):
        '''











        '''
        self.__dbg()

        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        if self.boardsStus[boardId] == False:
            self.__raise(f'board status error -> board {boardId} is false')

        jtagHandle = self.__board_handle_get_jtag(boardId)

        uartMgmt = self.m_board.FT232RManager()

        try:
            self.m_board.board_cpu_reset(jtagHandle, v_mode)
        except:
            uartMgmt.close()
            self.__raise(self.__except_traceback())

        uartMgmt.close()

#############################################################
# board ram load
#############################################################
    def f_ram_wr(self, v_wrFile, v_ramMode, v_verify=False, v_boardId=0):
        '''












        '''
        self.__dbg()

        self.__check_if_bin_valid(v_wrFile)

        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        if self.boardsStus[boardId] == False:
            self.__raise(f'board status error -> board {boardId} is false')

        if self.boardsType[boardId] == 'evb' and v_ramMode == 'rom':
            self.__raise(f'board {boardId} evb not support rom program')

        jtagHandle = self.__board_handle_get_jtag(boardId)

        try:
            self.m_jtag.jtag_set_check(False)
            self.m_board.board_ram_load(jtagHandle, v_wrFile, v_ramMode, v_verify=v_verify)
            self.m_jtag.jtag_set_check(True)
        except:
            self.m_jtag.jtag_set_check(True)
            self.__raise(self.__except_traceback())

    def f_ram_rd(self, v_rdFile, v_ramMode, v_boardId=0):
        '''











        '''
        self.__dbg()

        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        if self.boardsStus[boardId] == False:
            self.__raise(f'board status error -> board {boardId} is false')

        if v_ramMode == 'ilm':
            addr = 0xa0000000
        elif v_ramMode == 'rom':
            addr = 0x40000000
        else:
            self.__raise(f'board {boardId} ram type err -> {v_ramMode}')

        ramSize = 128 * 1024 #byte

        try:
            self.m_jtag.jtag_set_check(False)
            _, ramCtxList = self.f_csr_byte_rd(addr, ramSize, boardId)
            self.m_jtag.jtag_set_check(True)
        except:
            self.m_jtag.jtag_set_check(True)
            self.__raise(self.__except_traceback())

        with open(v_rdFile, 'wb') as f:
            f.write(bytes(ramCtxList))

#############################################################
# boot mode
#############################################################
    def f_boot_mode_set(self, v_bootMode, v_boardId=0):
        '''













        '''
        self.__dbg()

        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']

        if self.boardsType[boardId] == 'fpga':
            self.__raise(f'board {boardId} fpga not support boot mode set -> {v_bootMode}')

        if v_bootMode == 'flash':
            gpioSerboot = 1
            gpioSpiSelect = 1
        elif v_bootMode == 'eeprom':
            gpioSerboot = 1
            gpioSpiSelect = 0
        elif v_bootMode == 'dld':
            gpioSerboot = 0
            gpioSpiSelect = 0

        try:
            i2cHandle = self.__board_handle_get_i2c(boardId)

            gpioIVA = self.m_i2c.i2c_read_gpio_raw(i2cHandle)
            gpioIV = (gpioIVA & (self.BOOT_MODE_MASK)) | (gpioSerboot << 3) | (gpioSpiSelect << 6)
            self.m_i2c.i2c_write_gpio_raw(i2cHandle, 0xffff, gpioIV)
            self.__board_handle_close(boardId, 'I2C')
        except:
            self.__board_handle_close(boardId, 'I2C')
            self.__raise(self.__except_traceback())

#############################################################
# smbus addr
#############################################################
    def f_smbus_addr_set(self, v_smbusAddr, v_boardId=0):
        '''













        '''
        self.__dbg()

        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']

        if self.boardsType[boardId] == 'fpga':
            self.__raise(f'board {boardId} fpga not support smbus addr set')

        if (v_smbusAddr & 0xf8) != 0x20:
            self.__raise(f'smbus addr range is [0x20, 0x27]')

        gpioAddr0 = v_smbusAddr & 0x1
        gpioAddr1 = (v_smbusAddr >> 1) & 0x1
        gpioAddr2 = (v_smbusAddr >> 2) & 0x1

        jtagHandle = self.__board_handle_get_jtag(boardId)

        gpioJVA = self.m_jtag.jtag_read_gpio_raw(jtagHandle)

        gpioJV = (gpioJVA & (~self.SMBUS_ADDR_MASK)) | gpioAddr0 << 13 | gpioAddr1 << 14 | gpioAddr2 << 15
        self.m_jtag.jtag_write_gpio_raw(jtagHandle, 0xffff, gpioJV)
        self.__board_handle_close(boardId, 'JTAG')
        self.__board_handle_get_jtag(boardId)

#############################################################
# board id get
#############################################################
    def f_board_id_list_get(self):
        '''









        '''
        boardIdList = [boardInfo['boardid'] for boardInfo in self.boardsInfo]
        return boardIdList

    def f_board_id_get_by_sn(self, v_sn):
        '''












        '''
        boardIdList = []
        for boardInfo in self.boardsInfo:
            for chanInfo in boardInfo['channels']:
                if chanInfo['sn'] == v_sn:
                    boardIdList.append(boardInfo['boardid'])

        boardIdList = list(set(boardIdList))

        if len(boardIdList) == 0:
            return None
        elif len(boardIdList) == 1:
            return boardIdList[0]
        else:
            self.__raise(f'multi boards {boardIdList} with sn: {v_sn}')

    def f_board_id_get_by_locid(self, v_locid):
        '''












        '''
        boardIdList = []
        for boardInfo in self.boardsInfo:
            for chanInfo in boardInfo['channels']:
                if chanInfo['locid'] == v_locid:
                    boardIdList.append(boardInfo['boardid'])

        boardIdList = list(set(boardIdList))

        if len(boardIdList) == 0:
            return None
        elif len(boardIdList) == 1:
            return boardIdList[0]
        else:
            self.__raise(f'multi boards {boardIdList} with locid: {v_locid}')

#############################################################
# board type get
#############################################################
    def f_board_type_get(self, v_boardId=0):
        '''













        '''
        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        return self.boardsType[boardId]

#############################################################
# board status get
#############################################################
    def f_board_status_get(self, v_boardId=0):
        '''













        '''
        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']
        return self.boardsStus[boardId]

#############################################################
# dbg/log on or off
#############################################################
    def f_dbg_on(self):
        '''




        '''
        self.dbg = True

    def f_dbg_off(self):
        '''




        '''
        self.dbg = False

    def f_log_on(self):
        '''




        '''
        self.log = True

    def f_log_off(self):
        '''




        '''
        self.log = False

#############################################################
# i2c recover
#############################################################
    def f_eeprom_recover(self, v_boardId=0):
        '''












        '''
        self.__dbg()

        boardInfo = self.__board_info_get_by_id(v_boardId)
        boardId = boardInfo['boardid']

        if self.boardsStus[boardId] == False:
            self.__raise(f'board status error -> board {boardId} is false')

        jtagHandle = self.__board_handle_get_jtag(boardId)
        self.m_board.board_i2c_recover(jtagHandle)



#############################################################
# flash operation
#############################################################
    def f_size_format(self, v_nbytes):
        if v_nbytes >= 1024 * 1024:
            return f"{v_nbytes / (1024*1024):.1f} MB"
        elif v_nbytes >= 1024:
            return f"{v_nbytes / 1024:.1f} KB"
        else:
            return f"{v_nbytes} B"

    def __board_flash_get(self, v_boardId=0):
        import api.flash_operation as flash_operation
        handle = self.__board_handle_get_jtag(v_boardId)
        return flash_operation.Flash(handle, self)

    def f_flash_info(self, v_boardId=0):
        flash = self.__board_flash_get(v_boardId)
        return flash.f_info()

    def f_flash_status(self, v_boardId=0):
        flash = self.__board_flash_get(v_boardId)
        sr = flash.f_status()
        return {
            'raw' : sr,
            'wip' : (sr >> 0) & 1,
            'wel' : (sr >> 1) & 1,
            'bp'  : (sr >> 2) & 0xF,
            'qe'  : (sr >> 6) & 1,
            'srwd': (sr >> 7) & 1,
        }

    def f_flash_diag(self, v_boardId=0):
        flash = self.__board_flash_get(v_boardId)
        flash.f_diag()

    def f_flash_read(self, v_offset, v_length, v_output=None, v_print=True, v_boardId=0):
        flash = self.__board_flash_get(v_boardId)
        return flash.f_read(v_offset, v_length, v_output, v_print)

    def f_flash_write(self, v_offset, v_data, v_verify=False, v_boardId=0):
        flash = self.__board_flash_get(v_boardId)
        flash.f_write(v_offset, v_data, v_verify)

    def f_flash_erase(self, v_offset, v_length, v_boardId=0):
        flash = self.__board_flash_get(v_boardId)
        flash.f_erase(v_offset, v_length)

    def f_flash_burn(self, v_binFile, v_verify=None, v_offset=0x0, v_boardId=0):
        self.__check_if_bin_valid(v_binFile)
        flash = self.__board_flash_get(v_boardId)
        flash.f_burn(v_binFile, v_verify=v_verify, v_offset=v_offset)

    def f_flash_verify(self, v_binFile, v_offset,
                       v_verify='fast', v_boardId=0):
        self.__check_if_bin_valid(v_binFile)
        flash = self.__board_flash_get(v_boardId)
        flash.f_verify(v_binFile, v_offset, v_verify)

    def f_flash_blank_check(self, v_offset, v_length, v_boardId=0):
        flash = self.__board_flash_get(v_boardId)
        flash.f_blank_check(v_offset, v_length)

#############################################################
# eeprom operation
#############################################################
    def __board_eeprom_get(self, v_boardId=0, v_devAddr=None):
        import api.eeprom_operation as eeprom_operation
        hadnle = self.__board_handle_get_jtag(v_boardId)
        return eeprom_operation.Eeprom(hadnle, self, dev_addr=v_devAddr)

    def f_eeprom_scan(self, v_boardId=0, v_devAddr=None):
        eeprom = self.__board_eeprom_get(v_boardId, v_devAddr)
        return eeprom.f_scan()

    def f_eeprom_diag(self, v_boardId=0, v_devAddr=None):
        eeprom = self.__board_eeprom_get(v_boardId, v_devAddr)
        return eeprom.f_diag()

    def f_eeprom_read(self, v_offset, v_length, v_output=None, v_print=True,
                      v_boardId=0, v_devAddr=None):
        eeprom = self.__board_eeprom_get(v_boardId, v_devAddr)
        return eeprom.f_read(v_offset, v_length, v_output, v_print)

    def f_eeprom_write(self, v_offset, v_data, v_verify=False,
                       v_boardId=0, v_devAddr=None):
        eeprom = self.__board_eeprom_get(v_boardId, v_devAddr)
        eeprom.f_write(v_offset, v_data, v_verify)

    def f_eeprom_burn(self, v_binFile, v_offset=0x0, v_verify=False,
                      v_boardId=0, v_devAddr=None):
        self.__check_if_bin_valid(v_binFile)
        eeprom = self.__board_eeprom_get(v_boardId, v_devAddr)
        eeprom.f_burn(v_binFile, v_offset, v_verify)

    def f_eeprom_verify(self, v_binFile, v_offset,
                        v_boardId=0, v_devAddr=None):
        self.__check_if_bin_valid(v_binFile)
        eeprom = self.__board_eeprom_get(v_boardId, v_devAddr)
        eeprom.f_verify(v_binFile, v_offset)

#############################################################
# log decode (ww_log v1)
#############################################################
    def __board_log_get(self, v_map, v_boardId=0):
        import api.log_operation as log_operation
        # ensure the jtag channel is up (reads go through f_csr_byte_rd /
        # f_flash_read / f_eeprom_read, all of which need it)
        self.__board_handle_get_jtag(v_boardId)
        return log_operation.Log(self, v_map, boardId=v_boardId)

    def f_log_decode_ram(self, v_map, v_addr, v_length=4096,
                         v_raw=False, v_output=None, v_boardId=0):
        '''
        -> 直接读取掉电保持 RAM 区(DLM maintain region)的 encode 日志并 decode

        参数
            v_map    - 必选   ww_log_map.json 路径
            v_addr   - 必选   RAM 区内存地址(__dlm_log_start 的值)
            v_length - 可选   读取字节数, 默认 4096(一个 maintain region)
            v_raw    - 可选   每行后附带原始帧
            v_output - 可选   同时把原始 dump 存到该文件
        '''
        log = self.__board_log_get(v_map, v_boardId)
        return log.f_decode_ram(v_addr, v_length, v_raw, v_output)

    def f_log_decode_flash(self, v_map, v_offset=0x0, v_length=4096,
                           v_raw=False, v_output=None, v_boardId=0):
        '''
        -> 直接读取 flash 上 LOG 分区的 encode 日志(LOGH blocks)并 decode

        参数
            v_map    - 必选   ww_log_map.json 路径
            v_offset - 可选   flash 内偏移, 默认 0x0
            v_length - 可选   读取字节数, 默认 4096
            v_raw    - 可选   每行后附带原始帧
            v_output - 可选   同时把原始 dump 存到该文件
        '''
        log = self.__board_log_get(v_map, v_boardId)
        return log.f_decode_flash(v_offset, v_length, v_raw, v_output)

    def f_log_decode_eeprom(self, v_map, v_offset=0x0, v_length=4096,
                            v_raw=False, v_output=None,
                            v_boardId=0, v_devAddr=None):
        '''
        -> 直接读取 eeprom 上 LOG 分区的 encode 日志(LOGH blocks)并 decode

        参数
            v_map     - 必选   ww_log_map.json 路径
            v_offset  - 可选   eeprom 内偏移, 默认 0x0
            v_length  - 可选   读取字节数, 默认 4096
            v_raw     - 可选   每行后附带原始帧
            v_output  - 可选   同时把原始 dump 存到该文件
            v_devAddr - 可选   eeprom I2C 7-bit 地址(0x50-0x57), 不指定则自动扫描
        '''
        log = self.__board_log_get(v_map, v_boardId)
        return log.f_decode_eeprom(v_offset, v_length, v_devAddr,
                                   v_raw, v_output)
