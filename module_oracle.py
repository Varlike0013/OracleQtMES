def fetch_process():
    """
    查询 SAJET.SYS_PROCESS 表中指定的启用流程。
    返回 (msg,columns, rows)，其中msg,为信息 columns 为列名列表，rows 为数据行列表。
    若查询失败，返回 ([], [])。
    """
    # 固定的流程名称列表
    process_list = [
        'AOI', 'BAOI', 'BSPI', 'BSVI', 'PCB_INPUT', 'SMT_INPUT', 'SPI', 'SVI',
        'S_AOI', 'BottomVI', 'CHANGE_SN', 'CHECK_BAT', 'CHECK_CPU', 'CHECK_FAN',
        'DAOI', 'DICT', 'DInput', 'DOA_VI', 'FQC-CHK', 'HEATSINK', 'MDA', 'PLATE',
        'TopVI', 'CHK_LABEL1', 'Cutboard', 'F1Test', 'F2Test', 'F3Test', 'F4Test',
        'GLUE_SVI', 'Power On Test', 'CHECKSN', 'CHECK_BOX', 'CHKPART', 'CHKSSN',
        'CHK_MAC', 'CHK_PART', 'CQC', 'ColorCheck', 'OQC', 'OQC_F1', 'PACKING',
        'PBottomVI', 'PK_AOI', 'PK_VBATT', 'PTopVI', 'Packing1', 'PrintLabel',
        'QC_CHK', 'SOCPT0PVl'
    ]

    # 构建 SQL 和绑定变量
    # 使用动态占位符，例如 :p0, :p1, ...
    placeholders = ','.join([f':p{i}' for i in range(len(process_list))])
    sql = f"""
        SELECT *
        FROM SAJET.SYS_PROCESS P
        WHERE P.ENABLED = 'Y'
          AND P.PROCESS_NAME IN ({placeholders})
        ORDER BY P.STAGE_ID, P.PROCESS_NAME
    """
    # 绑定参数以字典形式提供
    bind_vars = {f'p{i}': name for i, name in enumerate(process_list)}

    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                cursor.execute(sql, bind_vars)
                rows = cursor.fetchall()
                columns = [desc[0] for desc in cursor.description] if rows else []
                return "OK",columns, rows
    except oracledb.Error as e:
        return f"数据库错误: {e}",[], []
    except Exception as e:
        return f"异常: {e}",[], []
def fetch_route_process(must_have_str, must_not_str):
    """
    调用存储过程 SAJET.FIND_ROUTE_PROCESS，根据必过/必不过流程列表查找满足条件的路由。
    :param must_have_str: 必过流程名称，逗号分隔，如 'F1Test,PBottomVI,PTopVI,PACKING,OQC'
    :param must_not_str:  必不过流程名称，逗号分隔，如 'F2Test,F4Test'
    :return: (msg, columns, rows)  msg 为 'OK' 或错误信息，columns 为 ['ROUTE_NAME']，rows 为路由名称列表
    """
    if not must_have_str and not must_not_str:
        return "必须提供至少一个流程列表", [], []
    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                res_var = cursor.var(oracledb.DB_TYPE_VARCHAR)
                cur_var = cursor.var(oracledb.DB_TYPE_CURSOR)
                cursor.callproc("SAJET.FIND_ROUTE_PROCESS", [must_have_str, must_not_str, res_var, cur_var])
                msg = res_var.getvalue()
                out_cursor = cur_var.getvalue()
                rows = []
                if out_cursor:
                    for row in out_cursor:
                        rows.append(row[0])
                else:
                    rows = []
                if msg != 'OK':
                    return msg, [], []
                columns = ['ROUTE_NAME']
                return "OK", columns, rows
    except oracledb.Error as e:
        return f"数据库错误: {e}", [], []
    except Exception as e:
        return f"异常: {e}", [], []
def copy_route_new(old_route,new_route,emp,is_overwrite=False):
    """
    调用存储过程 SAJET.SJ_COPY_ROUTE 复制流程。
    :param old_route: 原流程名称
    :param new_route: 新流程名称
    :param emp:       操作员工号
    :param is_overwrite: 是否覆盖已存在的流程，True 时传 'Y'，False 传 'N'
    :return: (message: str)
    """
    if not old_route or not new_route or not emp:
        return "参数不完整：原流程、新流程、工号均不能为空"

    overwrite_flag = 'Y' if is_overwrite else 'N'

    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                result_var = cursor.var(oracledb.DB_TYPE_VARCHAR)
                cursor.callproc("SAJET.SJ_COPY_ROUTE", [old_route, new_route, emp, overwrite_flag, result_var])
                result_msg = result_var.getvalue()
                return result_msg
    except oracledb.Error as e:
        return f"数据库错误: {e}"
    except Exception as e:
        return f"异常: {e}"
def fetch_work_order(work):
    """
    根据工单号查询工单详细信息。
    :param work: 工单号
    :return: (msg, columns, rows)
    WO_STATUS -> 0:initial,1:prepare,2:release,3:work in process,4:hold,5:cancel,6:complete
    """
    if not work:
        return "工单号不能为空", [], []

    sql = """
        SELECT W.WORK_ORDER,
            P.PART_NO,
            W.WO_RULE,
            W.VERSION,
            W.WO_STATUS,
            CASE 
                WHEN W.WO_STATUS = 0 THEN 'initial'
                WHEN W.WO_STATUS = 1 THEN 'prepare'
                WHEN W.WO_STATUS = 2 THEN 'release'
                WHEN W.WO_STATUS = 3 THEN 'work in process'
                WHEN W.WO_STATUS = 4 THEN 'hold'
                WHEN W.WO_STATUS = 5 THEN 'cancel'
                WHEN W.WO_STATUS = 6 THEN 'complete'
                ELSE 'unknown'
            END AS WO_STATUS_DESC,
            W.TARGET_QTY,
            W.INPUT_QTY,
            W.OUTPUT_QTY,
            R.ROUTE_NAME,
            PE.PROCESS_NAME AS START_PROCESS,
            PA.PROCESS_NAME AS END_PROCESSS
        FROM SAJET.G_WO_BASE W
        LEFT JOIN SAJET.SYS_PART P ON P.PART_ID = W.MODEL_ID
        LEFT JOIN SAJET.SYS_ROUTE R ON R.ROUTE_ID = W.ROUTE_ID
        LEFT JOIN SAJET.SYS_PROCESS PE ON PE.PROCESS_ID = W.START_PROCESS_ID
        LEFT JOIN SAJET.SYS_PROCESS PA ON PA.PROCESS_ID = W.END_PROCESS_ID
        WHERE W.WORK_ORDER = :work
    """
    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                cursor.execute(sql, work=work)
                rows = cursor.fetchall()
                columns = [desc[0] for desc in cursor.description] if rows else []
                return "OK", columns, rows
    except oracledb.Error as e:
        return f"数据库错误: {e}", [], []
    except Exception as e:
        return f"异常: {e}", [], []
def update_woder_status(wo, status):
    """
    更新工单的状态。
    :param wo:     工单号
    :param status: 新状态（数字，0-6）
    :return: 'OK' 或错误信息
    """
    if not wo:
        return "工单号不能为空"
    if status is None or not isinstance(status, int):
        return "状态值必须为整数"

    sql = """
        UPDATE SAJET.G_WO_BASE W
        SET W.WO_STATUS = :status
        WHERE W.WORK_ORDER = :wo
    """
    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                cursor.execute(sql, status=status, wo=wo)
                rowcount = cursor.rowcount
                conn.commit()
                if rowcount > 0:
                    return "OK"
                else:
                    return "未找到工单，更新失败"
    except oracledb.Error as e:
        return f"数据库错误: {e}"
    except Exception as e:
        return f"异常: {e}"
def fetch_smt_reelup(line, site, sn, time=3):
    """
    查询 SMT 飞达上料信息（远程表）。
    :param line: 线别 (STRLINEID)
    :param site: 站点 (STRSITE)
    :param sn:   飞达 SN (STRREELUPSN)
    :param time: 时间范围（天数），默认 3 天，查询 LOADRELLDATE >= TRUNC(SYSDATE)-time
    :return: (msg, columns, rows)
             msg: 'OK' 或错误信息
             columns: 列名列表
             rows: 数据行列表
    """
    if ((not line or not site) and not sn):
        return "请至少输入线别+站点组合，或输入SN", [], []

    # 时间参数验证
    try:
        time = int(time)
        if time < 0:
            return "时间范围不能为负数", [], []
    except (TypeError, ValueError):
        return "时间参数必须为整数", [], []

    sql = """
        SELECT *
        FROM TBL_SMT_REELUPINFO@smt T
        WHERE (T.STRLINEID = :line AND T.STRSITE = :site AND T.LOADRELLDATE >= TRUNC(SYSDATE) - :time)
           OR T.STRREELUPSN = :sn
    """
    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                cursor.execute(sql, line=line, site=site, sn=sn, time=time)
                rows = cursor.fetchall()
                columns = [desc[0] for desc in cursor.description] if rows else []
                return "OK", columns, rows
    except oracledb.Error as e:
        return f"数据库错误: {e}", [], []
    except Exception as e:
        return f"异常: {e}", [], []
def delete_smt_reelup(index, sn):
    """
    根据 NUMINDEX 删除 TBL_SMT_REELUPINFO@smt 中的记录。
    删除前检查该 SN 对应的记录总数，必须 >= 2 才允许删除（确保至少留有一条）。
    :param index: 记录主键索引 (NUMINDEX)
    :param sn:    料盘 SN (STRREELUPSN)
    :return: 'OK' 或错误信息字符串
    """
    if index is None:
        return "索引不能为空"
    if not sn:
        return "SN 不能为空"
    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                # 1. 查询该 SN 的记录总数
                count_sql = "SELECT COUNT(*) FROM TBL_SMT_REELUPINFO@smt WHERE STRREELUPSN = :sn"
                cursor.execute(count_sql, sn=sn)
                count = cursor.fetchone()[0]
                if count < 2:
                    return f"该料盘SN({sn})记录数不足，当前 {count} 条，至少需要2条才能删除"

                # 2. 执行删除
                del_sql = "DELETE FROM TBL_SMT_REELUPINFO@smt T WHERE T.NUMINDEX = :numindex"
                cursor.execute(del_sql, numindex=index)
                rowcount = cursor.rowcount
                conn.commit()
                if rowcount > 0:
                    return "OK"
                else:
                    return f"未找到 NUMINDEX={index} 的记录，删除失败"
    except oracledb.Error as e:
        return f"数据库错误: {e}"
    except Exception as e:
        return f"异常: {e}"
def fetch_lenovo_carton(carton):
    """
    查询联想装箱信息。若 carton 为空则查询全部，否则按箱号过滤。
    :param carton: 箱号
    :return: (msg, columns, rows)
             msg: 'OK' 或错误信息
             columns: 列名列表
             rows: 数据行列表
    """
    sql_base = """
        SELECT L.CARTON_NO,
               P.PART_NO,
               E.PDLINE_NAME,
               L.CLOSE_FLAG,
               L.CARTON_QTY,
               L.CREATE_TIME,
               L.TERMINAL_ID,
               L.OPTION_NUM1,
               T.TERMINAL_NAME
        FROM SAJET.G_PACK_CARTON_LENOVO L
        LEFT JOIN SAJET.SYS_PART P ON P.PART_ID = L.MODEL_ID
        LEFT JOIN SAJET.SYS_PDLINE E ON E.PDLINE_ID = L.PDLINE_ID
        LEFT JOIN SAJET.SYS_TERMINAL T ON T.TERMINAL_ID = L.TERMINAL_ID
    """

    if carton:
        sql = sql_base + " WHERE L.CARTON_NO = :carton"
        bind_vars = {'carton': carton}
    else:
        sql = sql_base
        bind_vars = {}

    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                cursor.execute(sql, bind_vars)
                rows = cursor.fetchall()
                columns = [desc[0] for desc in cursor.description] if rows else []
                return "OK", columns, rows
    except oracledb.Error as e:
        return f"数据库错误: {e}", [], []
    except Exception as e:
        return f"异常: {e}", [], []
def fetch_lenovo_carton_sn(carton):
    """
    查询联想箱号对应的 SN 列表。若 carton 为空则返回错误。
    :param carton: 箱号
    :return: (msg, columns, rows)
             msg: 'OK' 或错误信息
             columns: 列名列表
             rows: 数据行列表
    """
    if not carton:
        return "箱号不能为空", [], []

    sql = "SELECT * FROM SAJET.LENOVO_CARTON_SN WHERE CARTON_NO = :carton"
    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                cursor.execute(sql, carton=carton)
                rows = cursor.fetchall()
                columns = [desc[0] for desc in cursor.description] if rows else []
                return "OK", columns, rows
    except oracledb.Error as e:
        return f"数据库错误: {e}", [], []
    except Exception as e:
        return f"异常: {e}", [], []
def update_lenovo_status(carton, status):
    """
    更新联想装箱表的 CLOSE_FLAG 状态。
    :param carton: 箱号
    :param status: 状态值（CLOSE_FLAG）
    :return: 'OK' 或错误信息字符串
    """
    if not carton:
        return "箱号不能为空"
    if status is None:
        return "状态不能为空"

    sql = """
        UPDATE SAJET.G_PACK_CARTON_LENOVO L
        SET L.CLOSE_FLAG = :status
        WHERE L.CARTON_NO = :carton
    """
    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                cursor.execute(sql, status=status, carton=carton)
                rowcount = cursor.rowcount
                conn.commit()
                if rowcount > 0:
                    return "OK"
                else:
                    return f"未找到箱号 {carton}，更新失败"
    except oracledb.Error as e:
        return f"数据库错误: {e}"
    except Exception as e:
        return f"异常: {e}"
def delete_lenovo_carton_sn(carton):
    """
    先删除联想箱号对应的所有 SN 记录，成功后更新主表 OPTION_NUM1 为 0。
    :param carton: 箱号
    :return: 'OK' 或错误信息字符串
    """
    if not carton:
        return "箱号不能为空"

    delete_sql = "DELETE FROM SAJET.LENOVO_CARTON_SN WHERE CARTON_NO = :carton"
    update_sql = """
        UPDATE SAJET.G_PACK_CARTON_LENOVO
        SET OPTION_NUM1 = 0
        WHERE CARTON_NO = :carton
    """

    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                cursor.execute(delete_sql, carton=carton)
                cursor.execute(update_sql, carton=carton)
                conn.commit()
                return "OK"
    except oracledb.Error as e:
        return f"数据库错误: {e}"
    except Exception as e:
        return f"异常: {e}"
def insert_user_action(user_no, user_action, target=None, status=None, ip_address=None):
    """
    调用存储过程 SAJET.INSERT_VARLIKE_ACTION_LOG 插入用户操作日志。
    :param user_no:     工号（EMP_NO）
    :param user_action: 操作类型
    :param target:      操作目标（可选）
    :param status:      执行结果（可选）
    :param ip_address:  客户端IP（可选）
    :return: 'OK' 或错误信息字符串
    """
    if not user_no or not user_action:
        return "工号和操作类型不能为空"
    try:
        with oracledb.connect(user=username, password=password, dsn=dsn) as conn:
            with conn.cursor() as cursor:
                tres_var = cursor.var(oracledb.DB_TYPE_VARCHAR)
                cursor.callproc(
                    "SAJET.INSERT_VARLIKE_ACTION_LOG",
                    [user_no, user_action, target, status, ip_address, tres_var]
                )
                conn.commit()
                result = tres_var.getvalue()
                return result
    except oracledb.Error as e:
        return f"数据库错误: {e}"
    except Exception as e:
        return f"异常: {e}"