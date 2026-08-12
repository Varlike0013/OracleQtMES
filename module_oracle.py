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