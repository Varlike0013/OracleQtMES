from tkinter import ttk, messagebox,simpledialog
from collections import Counter
import tkinter as tk
import traceback
import os
import re
import sys
import csv
import time
import socket
import threading
import module_oracle
import module_login
import module_save
import module_timer
import module_thread
import module_version

# ---------- 全局变量（用于跨函数共享）----------
content_frame = None         # 内容区域框架
status_label = None          # 底部状态栏 Label
user_label = None            # 最底部用户信息栏
menubar = None               # 顶部菜单栏
current_tree = None          # 当前表格树
current_columns = None       # 当前表格表头
#流程相关
current_dip_route = None
current_pack_route = None
current_rework_route = None
#服务器IP
server_ip = None

# ---------- 资源载入 ---------
def get_all_ips():
    """
    获取本机所有 IPv4 地址（排除 127.0.0.1 和链路本地地址）。
    若无法获取，返回 ['0.0.0.0']。
    """
    ips = set()
    try:
        hostname = socket.gethostname()
        addrs = socket.getaddrinfo(hostname, None)
        for addr in addrs:
            ip = addr[4][0]
            # 过滤掉 IPv6 和本地回环
            if ':' in ip:  # IPv6 跳过
                continue
            if ip and ip != '127.0.0.1' and not ip.startswith('169.254'):  # 排除链路本地
                ips.add(ip)
    except:
        pass
    # 如果上述方法未获取到有效IP，尝试连接外部地址获取主IP
    if not ips:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                s.connect(("8.8.8.8", 80))
                primary = s.getsockname()[0]
                if primary and primary != '127.0.0.1':
                    ips.add(primary)
        except:
            pass

    return list(ips) if ips else ['0.0.0.0']
def get_ips_string():
    return ';'.join(get_all_ips())
def resource_path(relative_path):
    """获取打包后资源的绝对路径"""
    try:
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)
def copy_to_clipboard(text):
    root.clipboard_clear()
    root.clipboard_append(text)
def split_to_quoted_string(s):
    """输入 "a b c" 输出 "'a','b','c'"""
    parts = s.split()
    return ','.join([f"'{p}'" for p in parts]) if parts else ''
def clean_sn_input(sn_input):
    """分割输入，并去除每个部分的引号"""
    parts = re.split(r'[,\s]+', sn_input)
    return [p.strip().strip("'\"") for p in parts if p.strip()]
def is_valid_user(user):
    """判断是否符合D000000的格式，符合True"""
    return re.match(r'^D\d{6}$', user) is not None
def check_route_uniqueness(cols, rows):
    """
    检查查询结果中的流程名称是否唯一。
    若唯一，返回该流程名称；若存在多个不同流程，弹出错误并显示各流程名称及数量，返回 None。
    :param cols: 列名列表
    :param rows: 数据行列表
    :return: str or None
    """
    if not rows:
        return None
    try:
        route_idx = cols.index("ROUTE_NAME")
        route_list = [row[route_idx] for row in rows]
        route_counts = Counter(route_list)
        if len(route_counts) == 1:
            return route_list[0]  # 唯一的流程名称
        else:
            msg = "查询结果包含多个不同的流程：\n\n"
            for route, count in route_counts.items():
                msg += f"  {route}: {count} 条\n"
            messagebox.showerror("流程冲突", msg)
            return None
    except ValueError:
        # 若列中没有 ROUTE_NAME，忽略检查，返回 None
        return None
def parse_range_string(input_str):
    """
    解析范围字符串，如 '267G004134-267G004136'
    返回 (prefix, start_num, end_num, width) 或引发 ValueError。
    """
    if '-' not in input_str:
        raise ValueError("输入不包含 '-'，不是范围格式")
    left, right = input_str.split('-', 1)
    left, right = left.strip(), right.strip()
    if not left or not right:
        raise ValueError("范围格式错误，左右两边均不能为空")

    # 寻找公共前缀
    i = 0
    while i < len(left) and i < len(right) and left[i] == right[i]:
        i += 1
    if i == 0:
        raise ValueError("两个值没有公共前缀")
    prefix = left[:i]
    left_suffix = left[i:]
    right_suffix = right[i:]

    # 确保后缀是纯数字
    if not left_suffix.isdigit() or not right_suffix.isdigit():
        raise ValueError("后缀必须为纯数字")
    start_num = int(left_suffix)
    end_num = int(right_suffix)
    if start_num > end_num:
        raise ValueError(f"起始值({start_num})大于结束值({end_num})")

    # 宽度取两边后缀长度中的较大值（保证格式一致）
    width = max(len(left_suffix), len(right_suffix))
    return prefix, start_num, end_num, width
def build_menu():
        """根据登录状态构建菜单栏"""
        # 清空现有菜单
        global menubar
        menubar.delete(0, tk.END)

        if module_login.is_logined():
            # 问题处理
            question_menu = tk.Menu(menubar, tearoff=0)
            menubar.add_cascade(label="问题处理", menu=question_menu)
            question_menu.add_command(label="查询问题", command=query_issue)
            question_menu.add_command(label="查询设置", command=query_setting)
            # 用户（注销）
            menubar.add_command(label="注销", command=user_logout)
            # 重工流程
            route_menu = tk.Menu(menubar, tearoff=0)
            menubar.add_cascade(label="重工流程", menu=route_menu)
            route_menu.add_command(label="---查询---", state="disabled")
            route_menu.add_command(label="DIP$PACK", command=ui_routeR_dip_pack)
            route_menu.add_command(label="SN", command=ui_route_sn)
            route_menu.add_command(label="Process", command=ui_find_route_by_process)
            route_menu.add_command(label="---添加---", state="disabled")
            route_menu.add_command(label="DIP$PACK", command=ui_insert_routeR)
            route_menu.add_command(label="SN", command=ui_insert_route_sn)
            route_menu.add_command(label="复制流程", command=ui_copy_route)
            # 清除卡号
            mac_menu = tk.Menu(menubar, tearoff=0)
            menubar.add_cascade(label="清除卡号", menu=mac_menu)
            mac_menu.add_command(label="SN清除单板", command=ui_clear_mac)
            mac_menu.add_command(label="重工号清除多板", command=ui_clear_mac_rework)
            # 服务器IP
            server_menu = tk.Menu(menubar, tearoff=0)
            menubar.add_cascade(label="服务器IP", menu=server_menu)
            server_menu.add_command(label="查看IP列表", command=ui_tree_server)
            server_menu.add_command(label="加载全部IP(用于导出或者查询)", command=reload_server_ip)
            server_menu.add_command(label="导出全部IP", command=export_server_ip)
            # 查询
            query_menu = tk.Menu(menubar, tearoff=0)
            menubar.add_cascade(label="查询", menu=query_menu)
            query_menu.add_command(label="查询SN&PCB", command=query_sn_ppid)
            query_menu.add_command(label="查询SN&KEYPARTS", command=query_sn_keyparts)
            query_menu.add_command(label="查询箱号下的料件", command=qurey_keyparts_carton)
            query_menu.add_command(label="查询重工号下的料件", command=qurey_keyparts_rework)
            query_menu.add_command(label="查询工单状态", command=qurey_work_order)
            query_menu.add_command(label="查询SMT料盘", command=query_smt_reelup)
            query_menu.add_command(label="查询Lenovo箱号", command=query_lenovo_carton)
            query_menu.add_command(label="查询工单或料号料件(风扇或者散热片)", command=query_erp_material)
            # 其他
            other_menu = tk.Menu(menubar, tearoff=0)
            menubar.add_cascade(label="其他", menu=other_menu)
            other_menu.add_command(label="更新ASUS列印", command=update_erp_asus)
            other_menu.add_command(label="添加风扇或散热片料件", command=insert_wo_material)
            other_menu.add_command(label="检查版本更新", command=check_update_version)
            #重工
            menubar.add_command(label="重工执行", command=ui_rework)
        else:
            menubar.add_command(label="登录", command=re_login)
# ---------- 通用输入ui --------
def update_status_safe(text):
    """线程安全地更新状态栏"""
    if threading.current_thread() is threading.main_thread():
        status_label.config(text=text)
    else:
        root.after(0, lambda: status_label.config(text=text))
def update_user_label(text):
    user_label.config(text=f"{text};版本 {module_version.VERSION} 就绪")
def _ui_input_one(title, confirm_callback,
                  label="输入:", btn_text="确定", warning_msg="请输入内容"):
    """
    通用单输入界面
    :param title:             标题文本
    :param confirm_callback:  点击确定时的回调函数，接收 (value)
    :param label:             输入框前的标签文本
    :param btn_text:          确定按钮上的文本
    :param warning_msg:       输入框为空时的警告提示
    """
    global content_frame
    for widget in content_frame.winfo_children():
        widget.destroy()

    center_frame = tk.Frame(content_frame)
    center_frame.pack(expand=True)

    tk.Label(center_frame, text=title, font=("Arial", 12, "bold")).grid(row=0, column=0, columnspan=2, pady=10)

    tk.Label(center_frame, text=label).grid(row=1, column=0, padx=5, pady=5, sticky="e")
    entry = tk.Entry(center_frame)
    entry.grid(row=1, column=1, padx=5, pady=5, sticky="w")

    def on_confirm():
        value = entry.get().strip()
        if not value:
            messagebox.showwarning("提示", warning_msg)
            return
        confirm_callback(value)

    btn = tk.Button(center_frame, text=btn_text, command=on_confirm)
    btn.grid(row=2, column=0, columnspan=2, pady=10)
def _ui_input_two(title, confirm_callback,
                  label_1="输入1:", label_2="输入2:",
                  btn_text="确定", warning_msg="请至少输入一个条件"):
    """
    通用双输入界面
    :param title:             标题文本
    :param confirm_callback:  点击确定时的回调函数，接收 (value1, value2)
    :param label_1:           第一个输入框前的标签文本
    :param label_2:           第二个输入框前的标签文本
    :param btn_text:          确定按钮上的文本
    :param warning_msg:       两个输入框均为空时的警告提示
    """
    global content_frame
    for widget in content_frame.winfo_children():
        widget.destroy()

    center_frame = tk.Frame(content_frame)
    center_frame.pack(expand=True)

    tk.Label(center_frame, text=title, font=("Arial", 12, "bold")).grid(row=0, column=0, columnspan=2, pady=10)

    tk.Label(center_frame, text=label_1).grid(row=1, column=0, padx=5, pady=5, sticky="e")
    entry1 = tk.Entry(center_frame)
    entry1.grid(row=1, column=1, padx=5, pady=5, sticky="w")

    tk.Label(center_frame, text=label_2).grid(row=2, column=0, padx=5, pady=5, sticky="e")
    entry2 = tk.Entry(center_frame)
    entry2.grid(row=2, column=1, padx=5, pady=5, sticky="w")

    def on_confirm():
        val1 = entry1.get().strip()
        val2 = entry2.get().strip()
        if not val1 and not val2:
            messagebox.showwarning("提示", warning_msg)
            return
        confirm_callback(val1, val2)

    btn = tk.Button(center_frame, text=btn_text, command=on_confirm)
    btn.grid(row=3, column=0, columnspan=2, pady=10)
def _ui_input_three(title, confirm_callback,
                    label_1="输入1:", label_2="输入2:", label_3="输入3:",
                    btn_text="确定", warning_msg="请至少输入一个条件"):
    """
    通用三输入界面
    :param title:             标题文本
    :param confirm_callback:  点击确定时的回调函数，接收 (value1, value2, value3)
    :param label_1:           第一个输入框前的标签文本
    :param label_2:           第二个输入框前的标签文本
    :param label_3:           第三个输入框前的标签文本
    :param btn_text:          确定按钮上的文本
    :param warning_msg:       三个输入框均为空时的警告提示
    """
    global content_frame
    for widget in content_frame.winfo_children():
        widget.destroy()

    center_frame = tk.Frame(content_frame)
    center_frame.pack(expand=True)

    tk.Label(center_frame, text=title, font=("Arial", 12, "bold")).grid(row=0, column=0, columnspan=2, pady=10)

    tk.Label(center_frame, text=label_1).grid(row=1, column=0, padx=5, pady=5, sticky="e")
    entry1 = tk.Entry(center_frame)
    entry1.grid(row=1, column=1, padx=5, pady=5, sticky="w")

    tk.Label(center_frame, text=label_2).grid(row=2, column=0, padx=5, pady=5, sticky="e")
    entry2 = tk.Entry(center_frame)
    entry2.grid(row=2, column=1, padx=5, pady=5, sticky="w")

    tk.Label(center_frame, text=label_3).grid(row=3, column=0, padx=5, pady=5, sticky="e")
    entry3 = tk.Entry(center_frame)
    entry3.grid(row=3, column=1, padx=5, pady=5, sticky="w")

    def on_confirm():
        val1 = entry1.get().strip()
        val2 = entry2.get().strip()
        val3 = entry3.get().strip()
        if not val1 and not val2 and not val3:
            messagebox.showwarning("提示", warning_msg)
            return
        confirm_callback(val1, val2, val3)

    btn = tk.Button(center_frame, text=btn_text, command=on_confirm)
    btn.grid(row=4, column=0, columnspan=2, pady=10)
def ui_select_two_routes(columns, rows, on_selected, title="请选择恰好两个流程"):
    """
    通用路由选择界面，强制用户选择恰好两个路由（一个D开头，一个P开头），
    选择完成后通过回调返回 (dip, pack)。

    :param columns:      列名列表（如 ['ROUTE_ID', 'ROUTE_NAME']）
    :param rows:         数据行列表，每行为 (ROUTE_ID, ROUTE_NAME)
    :param on_selected:  回调函数，接收 (dip, pack) 两个参数
    :param title:        界面标题
    """
    global content_frame, status_label

    # 清空内容区域
    for widget in content_frame.winfo_children():
        widget.destroy()

    if not rows:
        tk.Label(content_frame, text="没有查询到流程信息", font=("Arial", 12)).pack(pady=20)
        status_label.config(text="无流程信息")
        return

    # ---------- 居中容器 ----------
    center_frame = tk.Frame(content_frame)
    center_frame.place(relx=0.5, rely=0.5, anchor="center")

    # 标题
    tk.Label(center_frame, text=title, font=("Arial", 12, "bold")).pack(pady=10)

    # 流程列表容器
    list_frame = tk.Frame(center_frame)
    list_frame.pack(pady=10)

    # 存储每个流程的 (BooleanVar, route_id, route_name)
    route_vars = []

    for idx, row in enumerate(rows):
        route_id = row[0] if row else ""
        route_name = row[1] if len(row) > 1 else ""
        var = tk.BooleanVar(value=False)

        row_frame = tk.Frame(list_frame)
        row_frame.pack(anchor="w", pady=2)

        cb = tk.Checkbutton(row_frame, variable=var,
                            command=lambda v=var: on_check(v))
        cb.pack(side=tk.LEFT)

        lbl = tk.Label(row_frame, text=f"{route_id} - {route_name}")
        lbl.pack(side=tk.LEFT, padx=5)

        route_vars.append((var, route_id, route_name))

    def on_check(clicked_var):
        """限制选中数量不超过2个"""
        selected = sum(1 for v, _, _ in route_vars if v.get())
        if selected > 2:
            clicked_var.set(False)
            messagebox.showwarning("选择限制", "只能选择两个流程，不能超过两个")
            selected = sum(1 for v, _, _ in route_vars if v.get())
        status_label.config(text=f"已选择 {selected} 个流程（必须恰好选2个）")

    def on_confirm():
        selected_routes = [(r_id, r_name) for v, r_id, r_name in route_vars if v.get()]
        if len(selected_routes) != 2:
            messagebox.showinfo("提示", "必须恰好选择两个流程")
            return

        # --- 新分配逻辑：P开头 → pack，另一个 → dip ---
        pack_candidates = [name for _, name in selected_routes if name.upper().startswith('P')]
        if len(pack_candidates) != 1:
            messagebox.showerror("分配失败", "请选择恰好一个P开头的流程，另一个为非P开头")
            return
        pack = pack_candidates[0]
        # 另一个给 dip
        dip = [name for _, name in selected_routes if name != pack][0]  # 剩余那个

        # 调用回调，传递选中的 dip 和 pack
        on_selected(dip, pack)

    # 确认按钮
    btn = tk.Button(center_frame, text="确认选择", command=on_confirm, width=12)
    btn.pack(pady=15)

    status_label.config(text="请选择恰好两个流程（D开头和P开头各一个）")
def ui_table_tree(cols, rows, menu_builder=None):
    """
    通用表格显示函数，支持自定义右键菜单（通过 menu_builder）。
    :param cols: 列名列表
    :param rows: 数据行列表
    :param menu_builder: 可选，自定义菜单构建函数，接收参数 (menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value)
                         如果不提供，则使用默认菜单（复制 + 查看流程）。
    """
    global content_frame, status_label, current_tree, current_columns

    # 清空内容区域
    for widget in content_frame.winfo_children():
        widget.destroy()

    if not rows:
        label = tk.Label(content_frame, text="没有查询到数据")
        label.pack(pady=20)
        current_time = time.strftime("%H:%M:%S")
        status_label.config(text=f" 查询无记录 | 部分右键功能需要登陆 | 执行时间: {current_time}")
        return

    # 创建表格框架
    frame = tk.Frame(content_frame)
    frame.pack(fill=tk.BOTH, expand=True)

    v_scrollbar = ttk.Scrollbar(frame)
    v_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
    h_scrollbar = ttk.Scrollbar(frame, orient=tk.HORIZONTAL)
    h_scrollbar.pack(side=tk.BOTTOM, fill=tk.X)

    tree = ttk.Treeview(frame, columns=cols, show="headings",
                        yscrollcommand=v_scrollbar.set,
                        xscrollcommand=h_scrollbar.set)
    tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    current_tree = tree
    current_columns = cols

    v_scrollbar.config(command=tree.yview)
    h_scrollbar.config(command=tree.xview)

    for col in cols:
        tree.heading(col, text=col)
        tree.column(col, width=100, anchor="center")

    for row in rows:
        tree.insert("", tk.END, values=row)

    current_time = time.strftime("%H:%M:%S")
    status_label.config(text=f" 查询结果：共 {len(rows)} 条记录 | 部分右键功能需要登陆 | 执行时间: {current_time}")

    # ---------- 右键菜单 ----------
    def show_popup(event):
        row_id = current_tree.identify_row(event.y)
        col_id = current_tree.identify_column(event.x)
        if not row_id or not col_id:
            return
        col_index = int(col_id[1:]) - 1
        col_name = current_columns[col_index]
        values = current_tree.item(row_id, 'values')
        cell_value = values[col_index] if col_index < len(values) else ""
        first_col_value = values[0] if values else None

        menu = tk.Menu(root, tearoff=0)
        if menu_builder:
            # 使用自定义菜单构建器
            menu_builder(menu, current_tree, current_columns, row_id, col_index, col_name, values, cell_value, first_col_value)
        else:
            # 默认菜单：复制当前值 + 查看当前值
            menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
            menu.add_command(label="查看当前值", command=lambda:show_cell_value(cell_value,col_name))
        menu.post(event.x_root, event.y_root)
    # ---------- 左键双击 ----------
    def on_double_click(event):
        row_id = current_tree.identify_row(event.y)
        col_id = current_tree.identify_column(event.x)
        if not row_id or not col_id:
            return
        col_index = int(col_id[1:]) - 1
        col_name = current_columns[col_index]
        values = current_tree.item(row_id, 'values')
        cell_value = values[col_index] if col_index < len(values) else ""
        # 默认：显示单元格值
        show_cell_value(cell_value,col_name)
    current_tree.bind("<Double-1>", on_double_click)
    current_tree.bind("<Button-3>", show_popup)
def show_cell_value(value,title="单元格值"):
    """
    弹窗显示单元格内容，支持复制（多行文本框，可选中）
    :param value: 要显示的内容
    """
    if not value:
        value = "(空)"
    win = tk.Toplevel(root)
    win.title(title)
    win.geometry("400x250")
    win.transient(root)
    win.grab_set()

    # 相对于主窗口居中
    root.update_idletasks()
    x = root.winfo_x() + (root.winfo_width() - 400) // 2
    y = root.winfo_y() + (root.winfo_height() - 250) // 2
    win.geometry(f"+{x}+{y}")

    # 多行文本框
    text = tk.Text(win, wrap=tk.WORD, height=10, width=50)
    text.pack(pady=10, padx=10, fill=tk.BOTH, expand=True)
    text.insert(tk.END, value)
    text.config(state=tk.DISABLED)  # 只读，但可选中复制
def show_notification(message):
    """
    在屏幕右下角显示通知窗口（多行信息）
    :param message: 要显示的消息文本（可包含换行符）
    """
    win = tk.Toplevel(root)
    win.overrideredirect(True)
    win.geometry("300x120")
    win.attributes("-topmost", True)
    screen_width = win.winfo_screenwidth()
    screen_height = win.winfo_screenheight()
    x = screen_width - 320
    y = screen_height - 200
    win.geometry(f"+{x}+{y}")
    frame = tk.Frame(win, bg="#e6f3ff", bd=2, relief=tk.RIDGE)
    frame.pack(fill=tk.BOTH, expand=True)
    icon = tk.Label(frame, text="❓", font=("Arial", 20), bg="#e6f3ff")
    icon.pack(side=tk.LEFT, padx=10, pady=10, anchor="n")
    text_label = tk.Label(frame, text=message, font=("微软雅黑", 10), bg="#e6f3ff",
                          wraplength=240, justify=tk.LEFT, anchor="nw")
    text_label.pack(side=tk.LEFT, padx=5, pady=10, fill=tk.BOTH, expand=True)
    btn_close = tk.Button(frame, text="✕", command=win.destroy, bd=0, bg="#e6f3ff", fg="#666", font=("Arial", 10))
    btn_close.pack(side=tk.RIGHT, padx=5, pady=5, anchor="n")
    win.after(8000, win.destroy)  # 8秒后自动关闭
    win.bind("<Button-1>", lambda e: win.destroy())
def ui_user_action(user_action, target=None, status=None, ip_address=get_ips_string()):
    """
    记录用户操作日志（UI层调用），自动检查登录状态并处理错误。
    :param user_action: 操作类型
    :param target:      操作目标（可选）
    :param status:      执行结果（可选）
    :param ip_address:  客户端IP（可选）
    """
    if not module_login.is_logined():
        messagebox.showwarning("未登录", "请先登录再执行此操作")
        status_label.config(text="操作取消：用户未登录")
        return
    msg = module_oracle.insert_user_action(module_login.current_user, user_action, target, status, ip_address)
    if msg != "OK":
        messagebox.showerror("日志记录失败", f"操作日志记录失败：{msg}")
        status_label.config(text="日志记录失败")
def _ui_input_three_with_combo(title, confirm_callback,
                               label1="输入1:", label2="输入2:", label3="输入3:",
                               combo_label="选择:", combo_options=None,
                               btn_text="确定", warning_msg="请完整输入"):
    """
    通用三输入框 + 下拉选择框界面
    :param title:              标题文本
    :param confirm_callback:   点击确定时的回调函数，接收 (value1, value2, value3, combo_selection)
    :param label1:             第一个输入框标签
    :param label2:             第二个输入框标签
    :param label3:             第三个输入框标签
    :param combo_label:        下拉框标签
    :param combo_options:      下拉选项列表，如 ['选项1', '选项2']
    :param btn_text:           确定按钮文字
    :param warning_msg:        输入为空时的警告提示
    """
    global content_frame
    # 清空内容区域
    for widget in content_frame.winfo_children():
        widget.destroy()

    center_frame = tk.Frame(content_frame)
    center_frame.pack(expand=True)

    # 标题
    tk.Label(center_frame, text=title, font=("Arial", 12, "bold")).grid(row=0, column=0, columnspan=2, pady=10)

    # 输入框1
    tk.Label(center_frame, text=label1).grid(row=1, column=0, padx=5, pady=5, sticky="e")
    entry1 = tk.Entry(center_frame)
    entry1.grid(row=1, column=1, padx=5, pady=5, sticky="w")

    # 输入框2
    tk.Label(center_frame, text=label2).grid(row=2, column=0, padx=5, pady=5, sticky="e")
    entry2 = tk.Entry(center_frame)
    entry2.grid(row=2, column=1, padx=5, pady=5, sticky="w")

    # 输入框3
    tk.Label(center_frame, text=label3).grid(row=3, column=0, padx=5, pady=5, sticky="e")
    entry3 = tk.Entry(center_frame)
    entry3.grid(row=3, column=1, padx=5, pady=5, sticky="w")

    # 下拉选择框
    tk.Label(center_frame, text=combo_label).grid(row=4, column=0, padx=5, pady=5, sticky="e")
    combo_var = tk.StringVar()
    if combo_options:
        combo_var.set(combo_options[0])  # 默认选择第一个
        combo = ttk.Combobox(center_frame, textvariable=combo_var, values=combo_options, state="readonly")
    else:
        combo = ttk.Combobox(center_frame, textvariable=combo_var, values=[], state="readonly")
    combo.grid(row=4, column=1, padx=5, pady=5, sticky="w")

    def on_confirm():
        val1 = entry1.get().strip()
        val2 = entry2.get().strip()
        val3 = entry3.get().strip()
        sel = combo_var.get()
        # 简单校验：若所有输入框为空，给出警告（可选择仅检查必要项，这里按需）
        if not val1 and not val2 and not val3:
            messagebox.showwarning("提示", warning_msg)
            return
        confirm_callback(val1, val2, val3, sel)

    btn = tk.Button(center_frame, text=btn_text, command=on_confirm)
    btn.grid(row=5, column=0, columnspan=2, pady=10)
# ---------- 问题处理功能  ----------
def query_issue(notify=False):
    """
    查询问题记录，显示表格，并自定义右键菜单（包含筛选、排序、回复OK等）
    :param notify: 是否启用通知
    """
    global content_frame, status_label
    try:
        columns, rows = module_oracle.fetch_rework_records()
    except Exception as e:
        messagebox.showerror("查询错误", str(e))
        return
    if not rows:
        ui_table_tree(columns, [])
        return
    if notify:
        config = module_save.load_config()
        show_dialog = config.get('settings', {}).get('misc', {}).get('show_confirmation_dialog', False)
        if show_dialog:
            max_row = max(rows, key=lambda r: r[0])
            line = max_row[1] if len(max_row) > 1 else ""
            problem = max_row[2] if len(max_row) > 2 else ""
            reason = max_row[3] if len(max_row) > 3 else ""
            requirement = max_row[4] if len(max_row) > 4 else ""
            reporter = max_row[6] if len(max_row) > 6 else ""

            info_lines = [
                f"线别: {line}",
                f"问题描述: {problem}",
                f"提交原因: {reason}",
                f"产线要求: {requirement}",
                f"提报人: {reporter}"
            ]
            detail = "\n".join(info_lines)
            if len(detail) > 200:
                detail = detail[:200] + "..."

            msg = f"发现 {len(rows)} 条新记录\n{detail}"
            show_notification(msg)
    # 自定义菜单构建器
    FORMAT_COPY_COLUMNS = ['SERIAL_NUMBER']
    def custom_menu(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
        menu.add_command(label="查看当前值", command=lambda: show_cell_value(cell_value,col_name))
        menu.add_command(label=f"筛选 '{cell_value}'", command=lambda: filter_by_column(col_name, cell_value, tree, columns))
        menu.add_separator()
        if col_name in FORMAT_COPY_COLUMNS:
            menu.add_command(label=f"格式复制 {format_copy_match(cell_value)}", command=lambda:copy_to_clipboard(format_copy_match(cell_value)))
            menu.add_command(label="格式查询", command=lambda:query_match(cell_value))
        menu.add_separator()
        menu.add_command(label="回复OK", command=lambda idx=first_col_value: review_question(idx, module_login.current_user,"OK"))
        menu.add_command(label="回复", command=lambda idx=first_col_value: review_question(idx, module_login.current_user))
    ui_table_tree(columns, rows, menu_builder=custom_menu)
def query_match(str_value):
    # 检测是否同时包含 SN 和 BGA 模式
    if re.search(r'SN\s*:', str_value, re.I) and re.search(r'BGA\s*:', str_value, re.I):
        query_format_sn_bga(format_copy_SN_BGA(str_value))
    elif re.search(r'SN\s*:', str_value, re.I) and re.search(r'PCB\s*:', str_value, re.I):
        query_format_sn_ppid(format_copy_SN_PCB(str_value))
    else:
        query_format_sn(format_copy_sn(str_value))
def query_format_sn_bga(sql_where):
    msg,columns,rows = module_oracle.fetch_format_sn_bga(sql_where)
    if msg == "OK":
        ui_table_tree(columns,rows)
    else:
        messagebox.showerror("失败", msg)
def query_format_sn_ppid(sql_where):
    msg,columns,rows = module_oracle.fetch_format_sn_ppid(sql_where)
    if msg == "OK":
        ui_table_tree(columns,rows)
    else:
        messagebox.showerror("失败", msg)
def query_format_sn(sql_where):
    msg,columns,rows = module_oracle.fetch_format_sn(sql_where)
    if msg == "OK":
        ui_table_tree(columns,rows)
    else:
        messagebox.showerror("失败", msg)
def format_copy_match(str_value):
    # 检测是否同时包含 SN 和 BGA 模式
    if re.search(r'SN\s*:', str_value, re.I) and re.search(r'BGA\s*:', str_value, re.I):
        return format_copy_SN_BGA(str_value)
    elif re.search(r'SN\s*:', str_value, re.I) and re.search(r'PCB\s*:', str_value, re.I):
        return format_copy_SN_PCB(str_value)
    else:
        return format_copy_sn(str_value)
def format_copy_sn(str_value):
    """
    将字符串按空格和换行分割，生成 SQL IN 子句。
    例如输入 "abc def ghi" 输出 "SERIAL_NUMBER IN ('abc','def','ghi')"
    """
    if not str_value:
        return ""
    # 按任意空白字符（空格、换行、制表符等）分割
    parts = re.split(r'\s+', str_value.strip())
    parts = [p for p in parts if p]  # 过滤空字符串

    if not parts:
        return ""
    # 去重并保留顺序
    unique_parts = list(dict.fromkeys(parts))
    # 生成 IN 子句
    values = "','".join(unique_parts)
    return f"SERIAL_NUMBER IN ('{values}')"
def format_copy_SN_PCB(str_value):
    """
    解析输入字符串中的 SN 和 PCB 值，生成 SQL 条件语句。
    支持格式：SN:xxx PCB:yyy 或 SN:xxx,PCB:yyy 等（以空格或逗号分隔）。
    示例输入：
        "SN:260675765830855 BGA:612352010U351410 SN:260675765830263 BGA:5CC705060U121402"
    输出：
        "STRSMTSN IN ('260675765830855','260675765830263') OR PCB_QRCODE IN ('612352010U351410','5CC705060U121402')"
    """
    if not str_value:
        return ""
    # 提取所有 SN 和 BGA 值（不区分大小写）
    sn_matches = re.findall(r'SN\s*:\s*([^\s,;]+)', str_value, re.IGNORECASE)
    pcb_matches = re.findall(r'PCB\s*:\s*([^\s,;]+)', str_value, re.IGNORECASE)
    # 去重并保留顺序（用 dict.fromkeys 保持顺序）
    sn_list = list(dict.fromkeys(sn_matches))
    pcb_list = list(dict.fromkeys(pcb_matches))
    conditions = []
    if sn_list:
        sn_values = "', '".join(sn_list)
        conditions.append(f"STRSMTSN IN ('{sn_values}')")
    if pcb_list:
        pcb_list = "', '".join(pcb_list)
        conditions.append(f"PCB_QRCODE IN ('{pcb_list}')")
    if not conditions:
        return ""
    return " OR ".join(conditions)
def format_copy_SN_BGA(str_value):
    """
    解析输入字符串中的 SN 和 BGA 值，生成 SQL 条件语句。
    支持格式：SN:xxx BGA:yyy 或 SN:xxx,BGA:yyy 等（以空格或逗号分隔）。
    示例输入：
        "SN:260675765830855 BGA:612352010U351410 SN:260675765830263 BGA:5CC705060U121402"
    输出：
        "SERIAL_NUMBER IN ('260675765830855','260675765830263') OR ITEM_PART_SN IN ('612352010U351410','5CC705060U121402')"
    """
    if not str_value:
        return ""
    # 提取所有 SN 和 BGA 值（不区分大小写）
    sn_matches = re.findall(r'SN\s*:\s*([^\s,;]+)', str_value, re.IGNORECASE)
    bga_matches = re.findall(r'BGA\s*:\s*([^\s,;]+)', str_value, re.IGNORECASE)
    # 去重并保留顺序（用 dict.fromkeys 保持顺序）
    sn_list = list(dict.fromkeys(sn_matches))
    bga_list = list(dict.fromkeys(bga_matches))
    conditions = []
    if sn_list:
        sn_values = "', '".join(sn_list)
        conditions.append(f"SERIAL_NUMBER IN ('{sn_values}')")
    if bga_list:
        bga_values = "', '".join(bga_list)
        conditions.append(f"ITEM_PART_SN IN ('{bga_values}')")
    if not conditions:
        return ""
    return " OR ".join(conditions)
def filter_by_column(col_name, value, tree, columns):
    """示例：根据列值过滤（实际应重新查询数据库）"""
    try:
        columns, rows = module_oracle.fetch_ECS_SN_REWORK_col_value(col_name,value)
    except Exception as e:
        messagebox.showerror("查询错误", str(e))
        return
def review_question(record_id,user,reply=None):
    """
    处理问题回复
    :param record_id: 记录ID（第一列值）
    :param user: 当前用户
    :param reply: 回复内容，若为 None 则弹出输入框让用户输入
    """
    if record_id is None:
        messagebox.showwarning("警告", "无法获取记录索引")
        return
    if reply is None:
        reply = simpledialog.askstring("输入回复", "请输入回复内容：", parent=root)
        if not reply:  # 用户取消或输入为空
            return
    if not messagebox.askyesno("确认", f"确定要将记录 [{record_id}] 标记为 [{reply}]  吗？"):
        return
    msg = module_oracle.update_question_status(record_id, reply, user)
    if msg == "OK":
        messagebox.showinfo("成功", msg)
        query_issue()  # 刷新
    else:
        messagebox.showerror("错误", f"更新失败: {msg}")
    ui_user_action("review_question",target=f"用户{user}回复问题{record_id}为{reply}",status=msg)
def query_setting():
    global content_frame, status_label

    # 清空内容区域
    for widget in content_frame.winfo_children():
        widget.destroy()

    setting_frame = tk.Frame(content_frame)
    setting_frame.pack(pady=30, padx=30)

    config = module_save.load_config()
    misc_cfg = config['settings'].get('misc', {})
    enabled = misc_cfg.get('auto_refresh_enabled', False)
    interval = misc_cfg.get('auto_refresh_interval', 300)
    is_show = misc_cfg.get('show_confirmation_dialog', False)
    floor_B2 = misc_cfg.get('floor_B2', True)   # 默认选中
    floor_B3 = misc_cfg.get('floor_B3', True)
    floor_B4 = misc_cfg.get('floor_B4', True)

    row = 0

    # ---- 自动刷新 ----
    tk.Label(setting_frame, text="自动刷新:", font=("Arial", 10)).grid(row=row, column=0, padx=5, pady=8, sticky="e")
    enabled_var = tk.BooleanVar(value=enabled)
    chk_enable = tk.Checkbutton(setting_frame, variable=enabled_var)
    chk_enable.grid(row=row, column=1, padx=5, pady=8, sticky="w")
    row += 1

    # ---- 问题提醒 ----
    tk.Label(setting_frame, text="问题提醒:", font=("Arial", 10)).grid(row=row, column=0, padx=5, pady=8, sticky="e")
    show_var = tk.BooleanVar(value=is_show)
    chk_show = tk.Checkbutton(setting_frame, variable=show_var)
    chk_show.grid(row=row, column=1, padx=5, pady=8, sticky="w")
    row += 1

    # ---- 楼层筛选 ----
    tk.Label(setting_frame, text="楼层筛选:", font=("Arial", 10)).grid(row=row, column=0, padx=5, pady=8, sticky="ne")  # 顶对齐
    # 三个复选框横向排列
    floor_frame = tk.Frame(setting_frame)
    floor_frame.grid(row=row, column=1, padx=5, pady=8, sticky="w")
    var_B2 = tk.BooleanVar(value=floor_B2)
    var_B3 = tk.BooleanVar(value=floor_B3)
    var_B4 = tk.BooleanVar(value=floor_B4)
    cb_B2 = tk.Checkbutton(floor_frame, text="B2", variable=var_B2)
    cb_B3 = tk.Checkbutton(floor_frame, text="B3", variable=var_B3)
    cb_B4 = tk.Checkbutton(floor_frame, text="B4", variable=var_B4)
    cb_B2.pack(side=tk.LEFT, padx=5)
    cb_B3.pack(side=tk.LEFT, padx=5)
    cb_B4.pack(side=tk.LEFT, padx=5)
    row += 1

    # ---- 刷新间隔 ----
    tk.Label(setting_frame, text="刷新间隔(秒):", font=("Arial", 10)).grid(row=row, column=0, padx=5, pady=8, sticky="e")
    interval_var = tk.IntVar(value=interval if interval > 0 else 300)
    spin_interval = tk.Spinbox(setting_frame, from_=1, to=3600, textvariable=interval_var, width=10)
    spin_interval.grid(row=row, column=1, padx=5, pady=8, sticky="w")
    row += 1

    # ---- 按钮 ----
    def on_confirm():
        new_enabled = enabled_var.get()
        new_interval = interval_var.get()
        new_show = show_var.get()
        new_B2 = var_B2.get()
        new_B3 = var_B3.get()
        new_B4 = var_B4.get()
        if new_enabled and new_interval <= 0:
            messagebox.showwarning("警告", "刷新间隔必须大于0")
            return
        # 更新全局变量（若有）
        global auto_refresh_enabled, refresh_interval
        auto_refresh_enabled = new_enabled
        refresh_interval = new_interval
        # 应用到定时器
        if new_enabled and new_interval > 0:
            module_timer.set_interval(new_interval)
            module_timer.start()
        else:
            module_timer.stop()
        # 保存配置
        config['settings']['misc']['auto_refresh_enabled'] = new_enabled
        config['settings']['misc']['auto_refresh_interval'] = new_interval
        config['settings']['misc']['show_confirmation_dialog'] = new_show
        config['settings']['misc']['floor_B2'] = new_B2
        config['settings']['misc']['floor_B3'] = new_B3
        config['settings']['misc']['floor_B4'] = new_B4
        module_save.save_config(config)
        status_label.config(text=f"设置已保存: 自动刷新={'启用' if new_enabled else '禁用'}, 间隔={new_interval}秒")
        # 返回主界面或显示确认信息
        for widget in content_frame.winfo_children():
            widget.destroy()
        tk.Label(content_frame, text="设置已保存", font=("Arial", 14)).pack(pady=50)

    btn_frame = tk.Frame(setting_frame)
    btn_frame.grid(row=row, column=0, columnspan=2, pady=20)
    btn_confirm = tk.Button(btn_frame, text="保存", command=on_confirm, width=8)
    btn_confirm.pack(side=tk.LEFT, padx=10)

    status_label.config(text="查询设置界面 - 修改后点击确定")
# ---------- 登录相关----------
class LoginDialog:
    """登录对话框，支持记住密码"""
    def __init__(self, parent, title="登录"):
        self.parent = parent
        self.dialog = tk.Toplevel(parent)
        self.dialog.title(title)
        self.dialog.geometry("350x220")
        self.dialog.resizable(False, False)
        self.dialog.transient(parent)
        self.dialog.grab_set()

        # ---------- 让对话框相对于父窗口居中 ----------
        parent.update_idletasks()  # 确保父窗口尺寸已计算
        parent_x = parent.winfo_x()
        parent_y = parent.winfo_y()
        parent_w = parent.winfo_width()
        parent_h = parent.winfo_height()
        dlg_w = 350
        dlg_h = 220
        x = parent_x + (parent_w - dlg_w) // 2
        y = parent_y + (parent_h - dlg_h) // 2
        self.dialog.geometry(f"+{x}+{y}")  # 仅移动位置，保持尺寸不变

        # ---------- 创建控件 ----------
        # 用户名
        tk.Label(self.dialog, text="用户名:").place(x=40, y=30)
        self.entry_username = tk.Entry(self.dialog)
        self.entry_username.place(x=110, y=30, width=180)

        # 密码
        tk.Label(self.dialog, text="密码:").place(x=40, y=70)
        self.entry_password = tk.Entry(self.dialog, show="*")
        self.entry_password.place(x=110, y=70, width=180)

        # 记住密码复选框
        self.remember_var = tk.BooleanVar()
        remember_cb = tk.Checkbutton(self.dialog, text="记住密码", variable=self.remember_var)
        remember_cb.place(x=110, y=110)

        # 登录按钮
        btn_login = tk.Button(self.dialog, text="登录", command=self.check_login, width=10)
        btn_login.place(x=130, y=150)

        # 绑定回车键
        self.dialog.bind('<Return>', lambda e: self.check_login())

        # 结果标志
        self.result = False
        self.user_info = None

        # 关闭窗口协议
        self.dialog.protocol("WM_DELETE_WINDOW", self.on_close)

        # ---------- 加载保存的凭据 ----------
        self.load_saved_credentials()

        # 模态等待
        self.parent.wait_window(self.dialog)

    def load_saved_credentials(self):
        """从 saves.json 加载上次保存的用户名和记住密码状态"""
        config = module_save.load_config()
        last_emp = config['user'].get('last_user_emp', '')
        if last_emp:
            self.entry_username.insert(0, last_emp)

        if config['user'].get('remember_me', False):
            self.remember_var.set(True)
            saved_pwd = config['user'].get('last_user_pws', '')
            if saved_pwd:
                self.entry_password.insert(0, saved_pwd)
        else:
            self.remember_var.set(False)

    def check_login(self):
        """验证用户输入，成功后关闭对话框"""
        emp = self.entry_username.get().strip()
        pws = self.entry_password.get()
        remember = self.remember_var.get()

        if not emp:
            messagebox.showerror("错误", "请输入用户名", parent=self.dialog)
            return

        # 调用 module_login 的 login 函数
        success, info = module_login.login(emp, pws, remember)
        if success:
            self.result = True
            self.user_info = info
            self.dialog.destroy()
            build_menu()
            ui_user_action("LoginDialog:check_login",target=f"用户{emp}登陆系统",status=info)
        else:
            messagebox.showerror("错误", f"登录失败：{info}", parent=self.dialog)
            self.entry_password.delete(0, tk.END)
            self.entry_username.focus()

    def on_close(self):
        """用户点击关闭按钮"""
        self.result = False
        self.dialog.destroy()
def re_login():
    global user_label, status_label, content_frame
    dlg = LoginDialog(root, title="登录")
    if dlg.result:
        # 更新界面
        update_user_label(f"当前用户: {module_login.current_user}")
        status_label.config(text="登录成功")
        for widget in content_frame.winfo_children():
            widget.destroy()
        welcome_label = tk.Label(content_frame, text=f"欢迎 {module_login.current_user}")
        welcome_label.pack(pady=20)
    else:
        pass
def user_logout():
    """注销后更新界面（可选）"""
    global user_label, content_frame
    ui_user_action("user_logout",target=f"用户{module_login.current_user}退出系统",status="OK")
    module_login.logout()
    build_menu()
    update_user_label("当前用户: 未登录")
    for widget in content_frame.winfo_children():
        widget.destroy()
    label = tk.Label(content_frame, text="已注销，请点击「登录」重新登录")
    label.pack(pady=20)
# -------- 重工流程相关 --------
def ui_routeR_dip_pack():
    def query_callback(dip, pack):
        result_msg, columns, rows = module_oracle.get_routeR_dip_pack(dip, pack)
        if result_msg == 'OK':
            if rows:
                global current_dip_route, current_pack_route
                current_dip_route = dip
                current_pack_route = pack
                ui_table_tree(columns, rows,route_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)

    _ui_input_two("查询DIP/PACK", query_callback,"DIP流程","PACK流程")
def route_menu(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
    menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
    if module_login.is_logined():
        menu.add_separator()
        if first_col_value:
            menu.add_command(label="查看流程", command=lambda: check_route(first_col_value))
        if current_dip_route and current_pack_route and first_col_value:
            menu.add_command(label="对比流程", command=lambda: contrast_route(current_dip_route, current_pack_route, first_col_value))
        else:
            menu.add_command(label="对比流程", state="disabled")
def contrast_route(dip,pack,rework):
    """
    对比两个流程（dip 和 pack）与 rework 流程的步骤
    左侧显示 dip + pack 的步骤合并列表，右侧显示 rework 步骤
    如果某流程无数据，则显示空表格
    """
    # 获取三个流程的数据（返回 (columns, rows)）
    _, rows_dip = module_oracle.fetch_route_steps(dip)
    _, rows_pack = module_oracle.fetch_route_steps(pack)
    _, rows_rework = module_oracle.fetch_route_steps(rework)

    # 合并左侧数据（dip + pack）
    rows_left = list(rows_dip) + list(rows_pack)

    # 创建对比窗口
    win = tk.Toplevel(root)
    win.title(f"流程对比: {dip}+{pack}  ↔  {rework}")
    win.geometry("800x500")
    win.transient(root)
    win.grab_set()

    # 使用 PanedWindow 左右分割
    paned = ttk.PanedWindow(win, orient=tk.HORIZONTAL)
    paned.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

    # ----- 左侧框架（dip + pack 合并）-----
    left_frame = tk.Frame(paned)
    paned.add(left_frame, weight=1)
    tk.Label(left_frame, text=f"流程: {dip} + {pack} (合并)", font=("Arial", 10, "bold")).pack(pady=5)
    l_scroll = ttk.Scrollbar(left_frame)
    l_scroll.pack(side=tk.RIGHT, fill=tk.Y)
    left_tree = ttk.Treeview(left_frame, columns=["步骤", "是否必过"], show="headings",
                             yscrollcommand=l_scroll.set)
    left_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    l_scroll.config(command=left_tree.yview)
    left_tree.heading("步骤", text="步骤")
    left_tree.heading("是否必过", text="是否必过")
    left_tree.column("步骤", width=150, anchor="center")
    left_tree.column("是否必过", width=80, anchor="center")
    for row in rows_left:
        left_tree.insert("", tk.END, values=row)

    # ----- 右侧框架（rework）-----
    right_frame = tk.Frame(paned)
    paned.add(right_frame, weight=1)
    tk.Label(right_frame, text=f"流程: {rework}", font=("Arial", 10, "bold")).pack(pady=5)
    r_scroll = ttk.Scrollbar(right_frame)
    r_scroll.pack(side=tk.RIGHT, fill=tk.Y)
    right_tree = ttk.Treeview(right_frame, columns=["步骤", "是否必过"], show="headings",
                              yscrollcommand=r_scroll.set)
    right_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    r_scroll.config(command=right_tree.yview)
    right_tree.heading("步骤", text="步骤")
    right_tree.heading("是否必过", text="是否必过")
    right_tree.column("步骤", width=150, anchor="center")
    right_tree.column("是否必过", width=80, anchor="center")
    for row in rows_rework:
        right_tree.insert("", tk.END, values=row)

    # 关闭按钮
    btn_close = tk.Button(win, text="关闭", command=win.destroy, width=10)
    btn_close.pack(pady=10)
def check_route(route_name):
    """查看流程详情（弹出新窗口）"""
    if not route_name:
        messagebox.showwarning("警告", "无效的流程标识")
        return

    # 查询步骤数据
    columns, rows = module_oracle.fetch_route_steps(route_name)
    if not columns or rows is None:
        messagebox.showerror("错误", "查询流程步骤失败")
        return

    if not rows:
        messagebox.showinfo("提示", "该流程没有步骤")
        return

    # 创建弹窗
    win = tk.Toplevel(root)
    win.title(f"流程步骤 - {route_name}")
    win.geometry("400x300")
    win.transient(root)
    win.grab_set()

    # 表格框架 + 滚动条
    frame = tk.Frame(win)
    frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

    v_scroll = ttk.Scrollbar(frame)
    v_scroll.pack(side=tk.RIGHT, fill=tk.Y)

    tree = ttk.Treeview(frame, columns=columns, show="headings",
                        yscrollcommand=v_scroll.set)
    tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    v_scroll.config(command=tree.yview)

    # 设置列
    for col in columns:
        tree.heading(col, text=col)
        tree.column(col, width=120, anchor="center")

    # 插入数据
    for row in rows:
        tree.insert("", tk.END, values=row)

    # 关闭按钮
    btn_close = tk.Button(win, text="关闭", command=win.destroy)
    btn_close.pack(pady=5)
def ui_route_sn():
    """查询 SN 对应的流程列表，并让用户选择两个流程后执行查询。"""
    def on_sn_entered(sn):
        if not sn:
            messagebox.showwarning("提示", "请输入 SN")
            return
        result_msg, columns, rows = module_oracle.get_route_sn(sn)
        if result_msg == 'OK':
            if rows:
                # 调用通用选择器，传入查询回调
                ui_select_two_routes(columns, rows,on_selected=on_routes_selected,title="请选择恰好两个流程进行查询")
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    def on_routes_selected(dip, pack):
        # 当用户选择好两个流程后，执行查询
        global current_dip_route, current_pack_route
        result_msg, columns, rows = module_oracle.get_routeR_dip_pack(dip, pack)
        if result_msg == 'OK':
            if rows:
                current_dip_route = dip
                current_pack_route = pack
                ui_table_tree(columns, rows,route_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    # 使用通用单输入界面获取 SN
    _ui_input_one(title="查询 SN 流程",confirm_callback=on_sn_entered,label="SN:",btn_text="确定",warning_msg="请输入 SN 后再查询")
def ui_routeR_sn(columns, rows):
    """
    显示 SN 查询到的流程列表，并强制用户选择恰好两个路由。
    :param columns: 列名列表（如 ['ROUTE_ID', 'ROUTE_NAME']）
    :param rows: 数据行列表，每行为 (ROUTE_ID, ROUTE_NAME)
    """
    global content_frame, status_label, current_dip_route, current_pack_route

    # 清空内容区域
    for widget in content_frame.winfo_children():
        widget.destroy()

    if not rows:
        tk.Label(content_frame, text="没有查询到流程信息", font=("Arial", 12)).pack(pady=20)
        status_label.config(text="无流程信息")
        return

    # ---------- 居中容器 ----------
    center_frame = tk.Frame(content_frame)
    center_frame.place(relx=0.5, rely=0.5, anchor="center")

    # 标题
    tk.Label(center_frame, text="请选择恰好两个流程进行查询:", font=("Arial", 12, "bold")).pack(pady=10)

    # 流程列表容器
    list_frame = tk.Frame(center_frame)
    list_frame.pack(pady=10)

    # 存储每个流程的 (BooleanVar, route_id, route_name)
    route_vars = []

    for idx, row in enumerate(rows):
        route_id = row[0] if row else ""
        route_name = row[1] if len(row) > 1 else ""
        var = tk.BooleanVar(value=False)

        row_frame = tk.Frame(list_frame)
        row_frame.pack(anchor="w", pady=2)

        cb = tk.Checkbutton(row_frame, variable=var,
                            command=lambda v=var: on_check(v))
        cb.pack(side=tk.LEFT)

        lbl = tk.Label(row_frame, text=f"{route_id} - {route_name}")
        lbl.pack(side=tk.LEFT, padx=5)

        route_vars.append((var, route_id, route_name))

    def on_check(clicked_var):
        """限制选中数量不超过2个"""
        selected = sum(1 for v, _, _ in route_vars if v.get())
        if selected > 2:
            clicked_var.set(False)
            messagebox.showwarning("选择限制", "只能选择两个流程，不能超过两个")
            selected = sum(1 for v, _, _ in route_vars if v.get())
        status_label.config(text=f"已选择 {selected} 个流程（必须恰好选2个）")

    def on_confirm():
        global current_dip_route, current_pack_route
        selected_routes = [(r_id, r_name) for v, r_id, r_name in route_vars if v.get()]
        if len(selected_routes) != 2:
            messagebox.showinfo("提示", "必须恰好选择两个流程")
            return

        # 分配名称：D开头给dip，P开头给pack
        dip = None
        pack = None
        for r_id, r_name in selected_routes:
            if r_name.startswith('D'):
                dip = r_name
            elif r_name.startswith('P'):
                pack = r_name

        if dip is None or pack is None:
            messagebox.showerror("分配失败", "请选择一个D开头的流程和一个P开头的流程")
            return

        # 执行查询
        result_msg, columns, rows = module_oracle.get_routeR_dip_pack(dip, pack)
        if result_msg == 'OK':
            if rows:
                current_dip_route = dip
                current_pack_route = pack
                ui_table_tree(columns, rows)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)

    # 确认按钮
    btn = tk.Button(center_frame, text="确认选择", command=on_confirm, width=12)
    btn.pack(pady=15)

    status_label.config(text="请选择恰好两个流程（D开头和P开头各一个）")
def ui_insert_routeR():
    def add_callback(dip, pack,router):
        result_msg = module_oracle.insert_routeR_DIP_PACK(dip, pack,router,module_login.current_user)
        if result_msg == 'OK':
            result_msg, columns, rows = module_oracle.get_routeR_dip_pack(dip, pack)
            if result_msg == 'OK':
                if rows:
                    global current_dip_route, current_pack_route
                    current_dip_route = dip
                    current_pack_route = pack
                    ui_table_tree(columns, rows)
                else:
                    messagebox.showinfo("查询结果", "没有查询到数据")
            else:
                messagebox.showerror("查询失败", result_msg)
        else:
            messagebox.showerror("查询失败", result_msg)
        ui_user_action("ui_insert_routeR",target=f"用户{module_login.current_user}添加新流程{router}；DIP流程为{dip}；PACK流程为{pack}；",status=result_msg)
    _ui_input_three("添加DIP&PACK的重工流程", add_callback,"DIP流程:","PACK流程:","重工流程:")
def ui_insert_route_sn():
    """添加新流程时，先让用户选择两个流程作为组合。"""
    def on_selected(dip, pack):
        # 用户选择完后，继续让用户输入新路由名称和工号
        global current_dip_route, current_pack_route
        def on_router_entered(router_name):
            emp = module_login.current_user
            msg = module_oracle.insert_routeR_DIP_PACK(dip, pack, router_name, emp)
            if msg == 'OK':
                messagebox.showinfo("添加成功", msg)
                contrast_route(dip,pack,router_name)
            else:
                messagebox.showerror("失败", msg)
            ui_user_action("ui_insert_routeR",target=f"用户{emp}添加新流程{router_name}；DIP流程为{dip}；PACK流程为{pack}；",status=msg)
        # 使用单输入获取新路由名称
        _ui_input_one("输入新流程名称", on_router_entered, label="流程名称:", btn_text="添加")
    def on_sn_entered(sn):
        if not sn:
            messagebox.showwarning("提示", "请输入 SN")
            return
        result_msg, columns, rows = module_oracle.get_route_sn(sn)
        if result_msg == 'OK':
            if rows:
                # 调用通用选择器，传入查询回调
                ui_select_two_routes(columns, rows,on_selected,title="选择要组合的两个流程")
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    # 使用通用单输入界面获取 SN
    _ui_input_one(title="查询 SN 流程",confirm_callback=on_sn_entered,label="SN:",btn_text="确定",warning_msg="请输入 SN 后再查询")
def ui_copy_route():
    """复制流程到新流程"""
    def on_route_entered(route1,route2):
        if not route1 and not route2:
            messagebox.showwarning("提示", "请输入 流程")
            return
        result_msg = module_oracle.copy_route_new(route1,route2,module_login.current_user)
        if result_msg == 'OK':
            messagebox.showinfo("执行结果", f"复制{route1}到{route2}成功")
        else:
            messagebox.showerror("复制失败", result_msg)
        ui_user_action("ui_copy_route",target=f"用户{module_login.current_user}复制旧流程{route1}到{route2}；",status=result_msg)
    _ui_input_two(title="复制流程到新流程",confirm_callback=on_route_entered,label_1="现有流程:",label_2="新流程：",btn_text="确定",warning_msg="请输入 流程名 后再尝试添加")
def ui_find_route_by_process():
    """
    显示流程列表，按 STAGE_ID 分组，支持折叠，每行可选择状态（无/必有/必无）
    PROCESS_NAME 作为第一列（树形文本），PROCESS_ID 隐藏但用于保存
    """
    global content_frame, status_label
    # 清空内容区域
    for widget in content_frame.winfo_children():
        widget.destroy()
    # 获取数据
    msg, columns, rows = module_oracle.fetch_process()
    if msg != 'OK' or not rows:
        error_msg = msg if msg != 'OK' else "没有获取到流程数据"
        tk.Label(content_frame, text=error_msg, font=("Arial", 12), fg="red").pack(pady=20)
        status_label.config(text=error_msg)
        return
    # 获取各字段索引
    col_map = {col: idx for idx, col in enumerate(columns)}
    proc_id_idx = col_map.get('PROCESS_ID')
    proc_name_idx = col_map.get('PROCESS_NAME')
    stage_id_idx = col_map.get('STAGE_ID')
    proc_desc_idx = col_map.get('PROCESS_DESC')
    if any(v is None for v in [proc_id_idx, proc_name_idx, stage_id_idx, proc_desc_idx]):
        messagebox.showerror("错误", "缺少必要的列：PROCESS_ID, PROCESS_NAME, STAGE_ID, PROCESS_DESC")
        return
    # ---------- STAGE_ID 名称映射 ----------
    stage_names = {
        20001: "SMT 段",
        20002: "DIP 段",
        20003: "测试段",
        20004: "包装段",
    }

    # 按 STAGE_ID 分组
    groups = {}
    for row in rows:
        stage_id = row[stage_id_idx]
        groups.setdefault(stage_id, []).append(row)

    # 创建 Treeview 树形结构
    tree_frame = tk.Frame(content_frame)
    tree_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

    # 列顺序：PROCESS_ID, PROCESS_DESC, 状态 (PROCESS_NAME 作为 tree 的 text)
    cols_display = ('PROCESS_ID', 'PROCESS_DESC', '状态')
    tree = ttk.Treeview(tree_frame, columns=cols_display, show='tree headings')
    tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    # 滚动条
    v_scroll = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=tree.yview)
    v_scroll.pack(side=tk.RIGHT, fill=tk.Y)
    tree.configure(yscrollcommand=v_scroll.set)

    # 列标题和左对齐
    for col in cols_display:
        tree.heading(col, text=col)
        tree.column(col, width=120, anchor='w')   # 左对齐

    # 存储每个子项的状态和 PROCESS_ID 映射
    item_states = {}      # iid -> 状态字符串
    item_proc_id = {}     # iid -> PROCESS_ID

    # 插入分组和子项
    for stage_id, items in groups.items():
        # 自定义组显示名称
        display_name = stage_names.get(stage_id, f"STAGE_{stage_id}")
        parent = tree.insert('', 'end', text=f'{display_name} (ID:{stage_id})', open=True)
        for row in items:
            proc_id = row[proc_id_idx]
            proc_name = row[proc_name_idx]
            proc_desc = row[proc_desc_idx] if proc_desc_idx is not None else ''
            # 将 PROCESS_NAME 作为 tree 的 text，其他放入 columns
            child = tree.insert(parent, 'end', text=proc_name, values=(proc_id, proc_desc, '无'))
            item_states[child] = '无'
            item_proc_id[child] = proc_id

    # ---------- 右键菜单：设置状态 ----------
    def show_popup(event):
        selected = tree.identify_row(event.y)
        if not selected:
            return
        # 仅对子节点（非组）显示状态菜单
        if tree.parent(selected) == '':
            return
        menu = tk.Menu(root, tearoff=0)
        for state in ['无', '必有', '必无']:
            menu.add_command(label=f'设为 {state}',
                             command=lambda s=selected, st=state: set_state(s, st))
        menu.post(event.x_root, event.y_root)

    def set_state(iid, state):
        values = list(tree.item(iid, 'values'))
        values[2] = state  # 状态列在 values 中的索引为 2
        tree.item(iid, values=values)
        item_states[iid] = state
        # 更新状态栏，显示流程名称
        proc_name = tree.item(iid, 'text')
        status_label.config(text=f'已设置 {proc_name} 状态为 {state}')

    tree.bind('<Button-3>', show_popup)

    def on_summit():
        # 收集状态为“必有”和“必无”的流程名称
        must_have = []
        must_not = []
        for iid, state in item_states.items():
            if state == '必有':
                proc_name = tree.item(iid, 'text')  # 流程名称作为 tree 的 text
                must_have.append(proc_name)
            elif state == '必无':
                proc_name = tree.item(iid, 'text')
                must_not.append(proc_name)
        # 拼接字符串
        must_have_str = ','.join(must_have)
        must_not_str = ','.join(must_not)
        msg,columns,rows = module_oracle.fetch_route_process(must_have_str,must_not_str)
        if msg =="OK":
            if rows:
                ui_table_tree(columns, rows,route_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", msg)
    btn_frame = tk.Frame(content_frame)
    btn_frame.pack(pady=10)
    tk.Button(btn_frame, text="确定", command=on_summit).pack(side=tk.LEFT, padx=10)
# --------- 清除卡号 ----------
def ui_clear_mac():
    """输入SN清除MAC卡号"""
    def on_sn_entered(sn):
        if not sn:
            messagebox.showwarning("提示", "请输入 SN")
            return
        result_msg, columns, rows = module_oracle.fetch_sn_mac(sn)
        if result_msg == 'OK':
            ui_table_tree(columns, rows,menu_mac)
        else:
            messagebox.showerror("查询失败", result_msg)
    def menu_mac(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        """自定义 MAC 表格右键菜单"""
        sn = values[columns.index('SERIAL_NUMBER')] if 'SERIAL_NUMBER' in columns else None
        mac = values[columns.index('MAC')] if 'MAC' in columns else None
        process = values[columns.index('WIP_PROCESS')] if 'MAC' in columns else None

        menu.add_command(label=f"复制 {col_name}", command=lambda: root.clipboard_append(cell_value))
        menu.add_command(label="查看当前值", command=lambda: show_cell_value(cell_value,col_name))

        if sn and mac and process=="F1Test":
            menu.add_separator()
            menu.add_command(label="删除 MAC",command=lambda s=sn, m=mac: delete_mac_action(s, m))
        else :
            menu.add_separator()
            menu.add_command(label="删除 MAC", state="disabled")

    def delete_mac_action(sn, mac):
        """执行删除 MAC 操作"""
        if not messagebox.askyesno("确认删除", f"确定要删除 SN={sn}, MAC={mac} 吗？"):
            return
        msg = module_oracle.insert_ht_mac(sn,mac)
        if msg!="OK":
            messagebox.showerror("失败", msg)
            return
        msg = module_oracle.delete_sn_mac(sn, mac)
        if msg=="OK":
            on_sn_entered(sn)
            messagebox.showinfo("成功", msg)
        else:
            messagebox.showerror("失败", msg)
        ui_user_action("delete_mac_action",target=f"删MAC卡号SN:{sn},MAC:{mac}；",status=msg)
    _ui_input_one(title="查询卡号 : ",confirm_callback=on_sn_entered,label="SN:",btn_text="确定",warning_msg="请输入 SN 后再查询")
def ui_clear_mac_rework():
    """清除重工号下的MAC卡号"""
    def on_input_entered(rewk):
        if not rewk:
            messagebox.showwarning("提示", "请输入 重工号")
            return
        result_msg, columns, rows = module_oracle.fetch_rework_mac(rewk)
        if result_msg == 'OK':
            ui_table_tree(columns, rows,menu_mac)
        else:
            messagebox.showerror("查询失败", result_msg)
    def menu_mac(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        """自定义 MAC 表格右键菜单"""
        sn = values[columns.index('SERIAL_NUMBER')] if 'SERIAL_NUMBER' in columns else None
        mac = values[columns.index('MAC')] if 'MAC' in columns else None

        menu.add_command(label=f"复制 {col_name}", command=lambda: root.clipboard_append(cell_value))
        menu.add_command(label="查看当前值", command=lambda: show_cell_value(cell_value,col_name))

        menu.add_separator()
        menu.add_command(label="删除 MAC",command=lambda s=sn, m=mac: delete_mac_action(s, m, tree))
    def delete_mac_action(rewk):
        """执行删除 MAC 操作"""
        if not messagebox.askyesno("确认删除", f"确定要删除查询到的全部记录吗？"):
            return
        msg = module_oracle.insert_ht_mac_rework(rewk)
        if msg!="OK":
            messagebox.showerror("失败", msg)
            return
        msg = module_oracle.delete_mac_rework(rewk)
        if msg=="OK":
            messagebox.showinfo("成功", msg)
        else:
            messagebox.showerror("失败", msg)
        ui_user_action("delete_mac_action",target=f"删MAC重工号:{rewk}；",status=msg)
    _ui_input_one(title="查询卡号 : ",confirm_callback=on_input_entered,label="重工号:",btn_text="确定",warning_msg="请输入 重工号 后再查询")
# -------- 服务器IP ----------
def load_server_ip():
    global server_ip
    server_ip = {}

    msg, cols, rows = module_oracle.fetch_server_gateway()
    if msg != 'OK' or not rows:
        update_status_safe("加载 IP 数据失败: " + msg)
        return

    col_map = {col: idx for idx, col in enumerate(cols)}
    for col in ['SERVER_ID', 'DRIVER_ID', 'GATEWAY_ID']:
        if col not in col_map:
            update_status_safe(f"缺少列: {col}")
            return

    total_ip_count = 0
    last_update_time = time.time()

    for row in rows:
        server_id = row[col_map['SERVER_ID']]
        driver_id = row[col_map['DRIVER_ID']]
        gateway_id = row[col_map['GATEWAY_ID']]
        gateway_key = f"{server_id}_{gateway_id}_{driver_id}"

        tres, ip_raw = module_oracle.fetch_gateway_ip(server_id, driver_id, gateway_id)
        if tres != 'OK' or not ip_raw:
            continue
        parts = ip_raw.split(';')
        ip_part = parts[1] if len(parts) > 1 else ''
        ip_list = [ip.strip() for ip in ip_part.split(',') if ip.strip()]
        if not ip_list:
            continue

        statuses = []
        for idx, ip in enumerate(ip_list, start=1):
            status_tres, _, status_rows = module_oracle.get_terminal_status(server_id, gateway_id, idx)
            if status_tres == 'OK' and status_rows:
                statuses.append(status_rows[0] if status_rows else None)
            else:
                statuses.append(None)

        for ip, status_row in zip(ip_list, statuses):
            segments = ip.split('.')
            subnet = '.'.join(segments[:3]) if len(segments) >= 3 else 'unknown'
            if subnet not in server_ip:
                server_ip[subnet] = {}
            if ip not in server_ip[subnet]:
                server_ip[subnet][ip] = []
            server_ip[subnet][ip].append((gateway_key, status_row))
            total_ip_count += 1

        # 每秒最多更新一次进度
        now = time.time()
        if now - last_update_time >= 1.0:
            update_status_safe(f"正在加载 IP 数据... 已加载 {total_ip_count} 个 IP")
            last_update_time = now

    update_status_safe(f"IP 数据加载完成，共 {total_ip_count} 个 IP")
def reload_server_ip():
    """
    后台加载全部 IP 数据，实时显示进度，加载完成后自动导出 CSV。
    使用模块级线程管理，防止重复执行。
    """
    task_id = "load_server_ip"
    if module_thread.is_running(task_id):
        messagebox.showinfo("提示", "导出任务正在执行中，请勿重复启动")
        return
    if server_ip:
        if not messagebox.askyesno("确认", "已存在IP数据，是否重新加载最新数据？"):
            status_label.config(text="已取消重新加载")
            return
    status_label.config(text="正在加载 IP 数据...")
    def on_finished(result, error):
        if error:
            messagebox.showerror("错误", f"加载IP失败: {error}")
        else:
            messagebox.showinfo("成功", f"IP数据加载完成")
    module_thread.start_task(
        task_id=task_id,
        target=load_server_ip,
        callback=on_finished
    )
def export_server_ip():
    """实际执行导出的函数（在主线程中运行）"""
    if not server_ip:
        messagebox.showinfo("提示", "未加载数据，请先加载IP数据")
        return
    try:
        desktop = os.path.expanduser("~/Desktop")
        filepath = os.path.join(desktop, "server_ip_export.csv")

        with open(filepath, 'w', newline='', encoding='utf-8-sig') as f:
            writer = csv.writer(f)
            writer.writerow(["网段", "IP", "网关Key(S_G_D)", "状态"])

            for subnet, ip_dict in server_ip.items():
                # 对每个网段内的 IP 进行数值排序
                valid_ips = []
                for ip in ip_dict.keys():
                    try:
                        parts = ip.split('.')
                        int_parts = [int(part) for part in parts]
                        valid_ips.append((ip, int_parts))
                    except ValueError:
                        pass
                sorted_ips = [ip for ip, _ in sorted(valid_ips, key=lambda x: x[1])]

                for ip in sorted_ips:
                    for gateway_key, status_row in ip_dict[ip]:
                        status = str(status_row) if status_row else "无状态"
                        writer.writerow([subnet, ip, gateway_key, status])

        status_label.config(text="导出完成")
        ui_user_action("export_server_ip",target=None,status= f"数据已导出到：{filepath}")
        messagebox.showinfo("导出成功", f"数据已导出到：{filepath}")
    except Exception as e:
        status_label.config(text="导出失败")
        ui_user_action("export_server_ip",target=None,status=f"写入CSV文件失败：{e}")
        messagebox.showerror("导出失败", f"写入CSV文件失败：{e}")
def ui_tree_server():
    """
    三层树结构：服务器 (Server) → 网关 (Gateway) → IP
    展开网关时自动调用存储过程查询 IP。
    """
    global content_frame, status_label,server_ip

    # 清空内容区域
    for widget in content_frame.winfo_children():
        widget.destroy()

    # 获取数据
    msg, columns, rows = module_oracle.fetch_server_gateway()
    if msg != 'OK' or not rows:
        error_text = msg if msg != 'OK' else "没有查询到数据"
        tk.Label(content_frame, text=error_text, font=("Arial", 12), fg="red").pack(pady=20)
        status_label.config(text=error_text)
        return

    # 获取必要的列索引
    col_map = {col: idx for idx, col in enumerate(columns)}
    required_cols = [
        'SERVER_ID', 'SERVER_DESC_E', 'DRIVER_ID',
        'GATEWAY_ID', 'GATEWAY_DESC_E', 'GAYTEWAY_CONNECT_NUMBER'
    ]
    for col in required_cols:
        if col not in col_map:
            messagebox.showerror("错误", f"缺少列: {col}")
            return

    # 按 SERVER_ID 分组，计算总连接数
    groups = {}
    server_total_conn = {}
    for row in rows:
        server_id = row[col_map['SERVER_ID']]
        groups.setdefault(server_id, []).append(row)
        conn_num = row[col_map['GAYTEWAY_CONNECT_NUMBER']] or 0
        server_total_conn[server_id] = server_total_conn.get(server_id, 0) + conn_num

    # 创建 Treeview
    tree = ttk.Treeview(content_frame, show='tree')
    tree.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

    # 存储网关参数 {iid: (server_id, driver_id, gateway_id)}
    gateway_params = {}

    # ----- 插入服务器和网关节点 -----
    for server_id, items in groups.items():
        server_desc = items[0][col_map['SERVER_DESC_E']]
        total_conn = server_total_conn.get(server_id, 0)
        server_text = f"{server_desc} (总连接数: {total_conn})"
        server_iid = tree.insert('', 'end', text=server_text, open=False)

        for row in items:
            gateway_desc = row[col_map['GATEWAY_DESC_E']]
            connect_num = row[col_map['GAYTEWAY_CONNECT_NUMBER']] or 0
            driver_id = row[col_map['DRIVER_ID']]
            gateway_id = row[col_map['GATEWAY_ID']]
            gateway_text = f"{gateway_desc} (连接数: {connect_num})"
            gateway_iid = tree.insert(server_iid, 'end', text=gateway_text, open=False)
            tree.insert(gateway_iid, 'end', text='加载中...', open=False)
            gateway_params[gateway_iid] = (server_id, driver_id, gateway_id)

    # ----- 展开网关时查询 IP -----
    def on_tree_open(event):
        item = tree.focus()
        if not item or item not in gateway_params:
            return
        children = tree.get_children(item)
        if children and tree.item(children[0], 'text') != '加载中...':
            return
        for child in children:
            tree.delete(child)

        server_id, driver_id, gateway_id = gateway_params[item]
        tres, ip_raw = module_oracle.fetch_gateway_ip(server_id, driver_id, gateway_id)
        if tres == 'OK' and ip_raw:
            try:
                parts = ip_raw.split(';')
                ip_part = parts[1] if len(parts) > 1 else ''
                ip_list = [ip.strip() for ip in ip_part.split(',') if ip.strip()]
                if ip_list:
                    for idx, ip in enumerate(ip_list, start=1):
                        _, _, status_rows = module_oracle.get_terminal_status(server_id, gateway_id, idx)
                        tree.insert(item, 'end', text=f"{idx}. {ip} {status_rows}", open=False)
                else:
                    tree.insert(item, 'end', text="IP: (无有效IP)", open=False)
            except Exception:
                tree.insert(item, 'end', text=f"IP 解析失败: {ip_raw[:50]}", open=False)
        elif tres == 'OK':
            tree.insert(item, 'end', text="IP: (空)", open=False)
        else:
            tree.insert(item, 'end', text=f"查询失败: {tres}", open=False)
    tree.bind('<<TreeviewOpen>>', on_tree_open)
# ---------- 查询 -----------
def query_sn_ppid():
    def query_callback(sn, pcb):
        result_msg, columns, rows = module_oracle.fetch_sn_ppid(sn, pcb)
        if result_msg == 'OK':
            if rows:
                ui_table_tree(columns, rows,ppid_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    def ppid_menu(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        sn = values[columns.index('STRSMTSN')] if 'STRSMTSN' in columns else None
        ppid = values[columns.index('PCB_QRCODE')] if 'PCB_QRCODE' in columns else None 

        menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
        if module_login.is_logined():
            menu.add_separator()
            menu.add_command(label="删除这一条记录", command=lambda: delete_sn_ppid(sn,ppid))
            if col_name == "STRSMTSN":
                menu.add_command(label="修改 STRSMTSN",command=lambda: prompt_update_sn(sn, ppid, field='sn'))
            if col_name == "PCB_QRCODE":
                menu.add_command(label="修改 PCB_QRCODE",command=lambda: prompt_update_sn(sn, ppid, field='ppid'))
    def delete_sn_ppid(sn,ppid):
        if not messagebox.askyesno("确认删除", f"确定要删除 SN={sn}, PCB二维码={ppid}的记录吗？"):
            return
        msg = module_oracle.delete_sn_ppid(sn, ppid)
        if msg == "OK":
            messagebox.showinfo("成功", "删除成功")
            query_callback(sn, ppid)
        else:
            messagebox.showerror("错误", f"删除失败: {msg}")
        ui_user_action("delete_sn_ppid",target=f"删除SN PPID,SN:{sn},PCB_QRCODE:{ppid}；",status=msg)
    def prompt_update_sn(sn, ppid, field):
        """弹出输入框获取新值并执行更新"""
        if field == 'sn':
            new_val = simpledialog.askstring("修改 STRSMTSN", "请输入新的 STRSMTSN:", parent=root)
            if new_val is None:  # 用户取消
                return
            new_val = new_val.strip()
            if not new_val:
                messagebox.showwarning("警告", "新 STRSMTSN 不能为空")
                return
            if not messagebox.askyesno("确认修改", f"确定将 STRSMTSN 从 {sn} 修改为 {new_val} 吗？"):
                return
            msg = module_oracle.update_ppid_sn(sn, ppid, new_sn=new_val)
            if msg == "OK":
                messagebox.showinfo("成功", "更新成功")
                query_callback(new_val,ppid)
            else:
                messagebox.showerror("错误", f"更新失败: {msg}")
            ui_user_action("prompt_update_sn",target=f"更新SN PPID,SN:{sn},PCB_QRCODE:{ppid}，新SN:{new_val}；",status=msg)
        else:  # ppid
            new_val = simpledialog.askstring("修改 PCB_QRCODE", "请输入新的 PCB_QRCODE:", parent=root)
            if new_val is None:
                return
            new_val = new_val.strip()
            if not new_val:
                messagebox.showwarning("警告", "新 PCB_QRCODE 不能为空")
                return
            if not messagebox.askyesno("确认修改", f"确定将 PCB_QRCODE 从 {ppid} 修改为 {new_val} 吗？"):
                return
            msg = module_oracle.update_ppid_pcb(sn, ppid, new_ppid=new_val)
            if msg == "OK":
                messagebox.showinfo("成功", "更新成功")
                query_callback(sn,new_val)
            else:
                messagebox.showerror("错误", f"更新失败: {msg}")
            ui_user_action("prompt_update_sn",target=f"更新SN PPID,SN:{sn},PCB_QRCODE:{ppid}，新PCB:{new_val}；",status=msg)
    _ui_input_two("查询 SN & PCB",query_callback,"SN :","PCB :")
def query_sn_keyparts():
    def query_callback(sn):
        result_msg, columns, rows = module_oracle.fetch_sn_keyparts(sn)
        if result_msg == 'OK':
            if rows:
                ui_table_tree(columns, rows,keyarts_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    def keyarts_menu(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        sn = values[columns.index('SERIAL_NUMBER')] if 'SERIAL_NUMBER' in columns else None
        part = values[columns.index('ITEM_PART_SN')] if 'ITEM_PART_SN' in columns else None 
        time = values[columns.index('UPDATE_TIME')] if 'UPDATE_TIME' in columns else None 

        menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
        if module_login.is_logined():
            menu.add_separator()
            menu.add_command(label="删除这一条记录", command=lambda: delete_sn_keypart(sn,part,time))
            menu.add_command(label="删除全部记录", command=lambda: delete_all_keyarts(sn))
    def delete_sn_keypart(sn, part, time):
        if not messagebox.askyesno("确认删除", f"确定要删除 SN={sn}, 料件SN={part}, 时间={time} 的记录吗？"):
            return
        msg = module_oracle.insert_ht_sn_keypart(sn, part, time)
        if msg != "OK":
            messagebox.showerror("错误", f"插入历史表失败: {msg}")
            return
        msg = module_oracle.delete_sn_keypart(sn, part, time)
        if msg == "OK":
            messagebox.showinfo("成功", "删除成功")
            query_callback(sn)
        else:
            messagebox.showerror("错误", f"删除失败: {msg}")
        ui_user_action("delete_sn_keypart",target=f"删除SN KEYPART料件,SN:{sn},KEYPART:{part}；",status=msg)
    def delete_all_keyarts(sn):
        if not messagebox.askyesno("确认删除", f"确定要删除 SN={sn} 的所有记录吗？"):
            return
        msg = module_oracle.insert_ht_sn_keyparts(sn)
        if msg != "OK":
            messagebox.showerror("错误", f"插入历史表失败: {msg}")
            return
        msg = module_oracle.delete_sn_keyparts(sn)
        if msg == "OK":
            messagebox.showinfo("成功", "删除成功")
            query_callback(sn)
        else:
            messagebox.showerror("错误", f"删除失败: {msg}")
        ui_user_action("delete_all_keyarts",target=f"删除SN全部料件,SN:{sn}；",status=msg)
    _ui_input_one("查询 SN & KEYPARTS",query_callback,"SN :")
def qurey_keyparts_carton():
    list_sn = []
    cartonc = None
    def query_callback(carton):
        nonlocal list_sn,cartonc
        cartonc = carton
        result_msg, columns, rows = module_oracle.fetch_carton_keyparts(carton)
        if result_msg == 'OK':
            if rows:
                try:
                    sn_index = columns.index("SERIAL_NUMBER")
                    list_sn = [row[sn_index] for row in rows]
                    list_sn = list(dict.fromkeys(list_sn))
                except ValueError:
                    list_sn = []
                ui_table_tree(columns, rows,keyarts_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    def keyarts_menu(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        wo = values[columns.index('WORK_ORDER')] if 'WORK_ORDER' in columns else None
        process = values[columns.index('PROCESS_NAME')] if 'PROCESS_NAME' in columns else None 
        menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
        if module_login.is_logined():
            menu.add_separator()
            menu.add_command(label=f"删除{wo}工单下{process}站位的记录", command=lambda: delete_keyparts_wo_process(wo,process,list_sn))
    def delete_keyparts_wo_process(wo,process,list_sn):
        if not messagebox.askyesno("确认删除", f"确定要删除工单{wo}下的{process}站位的记录吗？"):
            return
        msg = module_oracle.insert_ht_keyarts_wo_process(wo,process,list_sn)
        if msg != "OK":
            messagebox.showerror("错误", f"插入历史表失败: {msg}")
            return
        msg = module_oracle.delete_keyparts_wo_process(wo,process,list_sn)
        if msg == "OK":
            messagebox.showinfo("成功", "删除成功")
            query_callback(cartonc)
        else:
            messagebox.showerror("错误", f"删除失败: {msg}")
        ui_user_action("delete_keyparts_wo_process",target=f"查询条件箱号：{cartonc},删除{wo}工单下{process}站位的料件记录；",status=msg)
    _ui_input_one("查询 箱号下SN的料件 ",query_callback,"箱号 :")
def qurey_keyparts_rework():
    list_sn = []
    rework = None
    groups = {}
    def query_callback(rewk):
        nonlocal list_sn,rework,groups
        rework = rewk
        result_msg, columns, rows = module_oracle.fetch_keyparts_rework(rewk)
        if result_msg == 'OK':
            if rows:
                try:
                    sn_idx = columns.index("SERIAL_NUMBER")
                    wo_idx = columns.index("WORK_ORDER")
                    process_idx = columns.index("PROCESS_NAME")
                except ValueError:
                    messagebox.showerror("错误", "缺少必要的列")
                    return
                # 构建分组: key=(wo, process), value=list of sn
                groups = {}
                all_sn = []
                for row in rows:
                    sn = row[sn_idx]
                    wo = row[wo_idx]
                    process = row[process_idx]
                    all_sn.append(sn)
                    key = (wo, process)
                    groups.setdefault(key, []).append(sn)

                # 去重
                list_sn = list(dict.fromkeys(all_sn))
                for key in groups:
                    groups[key] = list(dict.fromkeys(groups[key]))
                # 构建显示数据：每行 (WORK_ORDER, PROCESS_NAME, 合并的SN字符串)
                display_rows = []
                for (wo, process), sn_list in groups.items():
                    sns_str = ', '.join(sn_list)
                    display_rows.append((wo, process, sns_str))
                display_columns = ['WORK_ORDER', 'PROCESS_NAME', 'SERIAL_NUMBER']
                ui_table_tree(display_columns, display_rows, keyarts_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    def keyarts_menu(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        wo = values[columns.index('WORK_ORDER')] if 'WORK_ORDER' in columns else None
        process = values[columns.index('PROCESS_NAME')] if 'PROCESS_NAME' in columns else None 
        menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
        if module_login.is_logined():
            menu.add_separator()
            menu.add_command(label=f"删除{wo}工单下{process}站位的记录", command=lambda: delete_keyparts_wo_process(wo,process,list_sn))
    def delete_keyparts_wo_process(wo,process,list_sn):
        if not messagebox.askyesno("确认删除", f"确定要删除工单{wo}下的{process}站位的记录吗？"):
            return
        msg = module_oracle.insert_ht_keyarts_wo_process(wo,process,list_sn)
        if msg != "OK":
            messagebox.showerror("错误", f"插入历史表失败: {msg}")
            return
        msg = module_oracle.delete_keyparts_wo_process(wo,process,list_sn)
        if msg == "OK":
            messagebox.showinfo("成功", "删除成功")
            query_callback(rework)
        else:
            messagebox.showerror("错误", f"删除失败: {msg}")
        ui_user_action("delete_keyparts_wo_process",target=f"查询条件重工号：{rework},删除{wo}工单下{process}站位的料件记录；",status=msg)
    _ui_input_one("查询 重工编号下的SN的料件 ",query_callback,"重工编号 :")
def qurey_work_order():
    def query_callback(work):
        result_msg, columns, rows = module_oracle.fetch_work_order(work)
        if result_msg == 'OK':
            if rows:
                ui_table_tree(columns, rows,order_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    def order_menu(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        wo = values[columns.index('WORK_ORDER')] if 'WORK_ORDER' in columns else None

        menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
        if module_login.is_logined() and wo:
            menu.add_separator()
            # 创建状态子菜单
            status_menu = tk.Menu(menu, tearoff=0)
            menu.add_cascade(label="修改工单状态", menu=status_menu)
            status_map = {
                0: "initial",
                1: "prepare",
                2: "release",
                3: "work in process",
                4: "hold",
                5: "cancel",
                6: "complete"
            }
            for status_code, status_desc in status_map.items():
                status_menu.add_command(
                    label=f"{status_code} - {status_desc}",
                    command=lambda s=status_code: update_workorder_status(wo, s)
                )
    def update_workorder_status(wo,status):
        if not messagebox.askyesno("确认修改", f"确定要修改工单{wo}的状态为{status}吗？"):
            return
        msg = module_oracle.update_woder_status(wo,status)
        if msg == "OK":
            messagebox.showinfo("成功", "修改成功")
            query_callback(wo)
        else:
            messagebox.showerror("错误", f"修改失败: {msg}")
        ui_user_action("update_workorder_status",target=f"修改工单状态：修改工单{wo}的状态为{status}；",status=msg)
    _ui_input_one("查询工单状态 ",query_callback,"工单 :")
def query_smt_reelup():
    def query_callback(line,site,sn):
        result_msg, columns, rows = module_oracle.fetch_smt_reelup(line,site,sn)
        if result_msg == 'OK':
            if rows:
                ui_table_tree(columns, rows,reel_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    def reel_menu(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        index = values[columns.index('NUMINDEX')] if 'NUMINDEX' in columns else None
        user = values[columns.index('STRLOADUSER')] if 'STRLOADUSER' in columns else None  
        reelsn = values[columns.index('STRREELUPSN')] if 'STRREELUPSN' in columns else None  

        menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
        menu.add_separator()
        if is_valid_user(user):
            menu.add_command(label=f"删除工号为{user}的料号为{reelsn}的记录",state="disabled")
        else:
            menu.add_command(label=f"删除工号为{user}的料号为{reelsn}的记录",command=lambda: delete_smt_reelup(index,reelsn))
    def delete_smt_reelup(index,reelsn):
        if not messagebox.askyesno("确认删除", f"确定要删除索引为{index}的记录吗？"):
            return
        msg = module_oracle.delete_smt_reelup(index,reelsn)
        if msg == "OK":
            messagebox.showinfo("成功", "删除成功")
            query_callback(None,None,reelsn)
        else:
            messagebox.showerror("错误", f"删除失败: {msg}")
        ui_user_action("delete_smt_reelup",target=f"删除索引为{index},料盘为{reelsn}的记录吗；",status=msg)
    _ui_input_three("查询SMT上料",query_callback," 线别 : "," 站位 : "," 料盘SN : ")
    update_status_safe("线别类似:B33F01LB&B303F01L,站位:1-100,输入线别和站位,或者单独输入料盘SN")
def query_lenovo_carton():
    def query_callback(carton):
        result_msg, columns, rows = module_oracle.fetch_lenovo_carton(carton)
        if result_msg == 'OK':
            if rows:
                ui_table_tree(columns, rows,carton_menu)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    def carton_menu(menu, tree, columns, row_id, col_index, col_name, values, cell_value, first_col_value):
        carton_no = values[columns.index('CARTON_NO')] if 'CARTON_NO' in columns else None
        menu.add_command(label=f"复制 {col_name}", command=lambda: copy_to_clipboard(cell_value))
        menu.add_command(label=f"查看箱号{carton_no}内的板",command=lambda: query_lenovo_carton_sn(carton_no))
        menu.add_command(label=f"清空箱号{carton_no}内的板",command=lambda: clear_lenovo_carton_sn(carton_no))
        if module_login.is_logined():
            menu.add_separator()
             # 创建状态子菜单
            status_menu = tk.Menu(menu, tearoff=0)
            menu.add_cascade(label="修改箱号状态", menu=status_menu)
            status_map = {
                "Y": "Close",
                "U": "UnHold",
                "N": "UnClose",
            }
            for status_code, status_desc in status_map.items():
                status_menu.add_command(
                    label=f"{status_code} - {status_desc}",
                    command=lambda s=status_code: update_lenovo_status(carton_no, s)
                )
    def query_lenovo_carton_sn(carton):
        result_msg, columns, rows = module_oracle.fetch_lenovo_carton_sn(carton)
        if result_msg == 'OK':
            if rows:
                ui_table_tree(columns, rows)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    def update_lenovo_status(carton_no,status):
        if not messagebox.askyesno("确认修改", f"确定要修改箱号{carton_no}的状态为{status}吗？"):
            return
        msg = module_oracle.update_lenovo_status(carton_no,status)
        if msg == 'OK':
            query_callback(carton_no)
        else:
            messagebox.showerror("查询失败", msg)
        ui_user_action("update_lenovo_status",target=f"修改箱号{carton_no}的状态为{status}；",status=msg)
    def clear_lenovo_carton_sn(carton):
        if not messagebox.askyesno("确认删除", f"确定要清空箱号{carton}内的板吗？"):
            return
        msg = module_oracle.delete_lenovo_carton_sn(carton)
        if msg == 'OK':
            query_callback(carton)
        else:
            messagebox.showerror("查询失败", msg)
        ui_user_action("clear_lenovo_carton_sn",target=f"清空箱号{carton}内的板；",status=msg)
    _ui_input_one("查询联想箱号",query_callback,"输入箱号：")
def query_erp_material():
    def query_callback(carton):
        result_msg, columns, rows = module_oracle.fetch_erp_material(carton)
        if result_msg == 'OK':
            if rows:
                ui_table_tree(columns, rows)
            else:
                messagebox.showinfo("查询结果", "没有查询到数据")
        else:
            messagebox.showerror("查询失败", result_msg)
    _ui_input_one("查询风扇或散热片料件",query_callback,"输入料号或者工单：")
# ---------- 其他 ------------
def update_erp_asus():
    """
    启动 ERP 更新任务（后台线程），利用 module_thread 防止重复执行。
    """
    task_id = "update_erp_asus"
    if module_thread.is_running(task_id):
        messagebox.showinfo("提示", "ERP更新任务正在执行中，请勿重复启动")
        return
    status_label.config(text="正在更新 ERP 数据，请稍候...")
    def on_finished(result, error):
        if error:
            messagebox.showerror("错误", f"同步失败: {error}")
            status_label.config(text="同步失败")
        elif result == "OK":
            messagebox.showinfo("成功", "ERP 数据同步完成")
            status_label.config(text="同步完成")
        else:
            messagebox.showerror("错误", f"同步失败: {result}")
            status_label.config(text="同步失败")
        ui_user_action(task_id,target=None,status=result)
    module_thread.start_task(
        task_id=task_id,
        target=module_oracle.update_erp_asus,
        callback=on_finished
    )
def insert_wo_material():
    def insert_callback(wopart, ecs_part, kpart, decs):
        msg = module_oracle.insert_erp_material(wopart,ecs_part,decs,kpart)
        if msg == 'OK':
            messagebox.showinfo("查询结果", "添加成功")
        else:
            messagebox.showerror("查询失败", msg)
        ui_user_action("insert_wo_material",target=f"工单或者料号：{wopart}；ECS料号：{ecs_part}；料件料号：{kpart}；料件类型：{decs}；",status=msg)
    _ui_input_three_with_combo(
        title="添加风扇或散热片料件",
        label1="工单号或者料号：",
        label2="ECS料号:",
        label3="风扇或散热料号：",
        combo_label="状态",
        combo_options=["FAN SET", "HEAT PIPE", "PLATE"],
        confirm_callback=insert_callback,
        warning_msg="请至少输入一个条件"
    )
def check_update_version():
    status, msg = module_version.check_version_status()
    if status == 1:
        if messagebox.askyesno("更新提示", f"{msg}\n\n是否立即更新？"):
            success, msg = module_version.perform_update()
            if success:
                if "重启" in msg:
                    sys.exit(0)  # 退出当前进程，让批处理接管
                else:
                    messagebox.showinfo("更新完成", msg)
            else:
                messagebox.showerror("更新失败", msg)
    elif status == -1:
        messagebox.showerror("检查更新失败", msg) # 无法获取云端版本
def ui_rework():
    """重工执行界面：左侧输入，右侧表格"""
    global content_frame
    query_data = {}
    column_map = {
        "序号": "SERIAL_NUMBER",
        "箱号": "CARTON_NO",
        "工单": "WORK_ORDER",
        "重工号": "REWORK_NO",
        "QC号": "QC_NO"
    }
    for widget in content_frame.winfo_children():
        widget.destroy()

    paned = ttk.PanedWindow(content_frame, orient=tk.HORIZONTAL)
    paned.pack(fill=tk.BOTH, expand=True)

    # ----- 左侧面板：输入区域 -----
    left_frame = tk.Frame(paned)
    paned.add(left_frame, weight=1)

    input_panel = tk.Frame(left_frame)
    input_panel.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
    # 新工单（复选框 + 输入框）
    row_new_wo = tk.Frame(input_panel)
    row_new_wo.pack(fill=tk.X, pady=5)
    new_wo_var = tk.BooleanVar()
    cb_new_wo = tk.Checkbutton(row_new_wo, text="新工单", variable=new_wo_var)
    cb_new_wo.pack(side=tk.LEFT, padx=5)
    entry_new_wo = tk.Entry(row_new_wo, width=20)
    entry_new_wo.pack(side=tk.LEFT, padx=5)
    # 重工单号 + 新增按钮
    row1 = tk.Frame(input_panel)
    row1.pack(fill=tk.X, pady=5)
    tk.Label(row1, text="重工单号", width=10, anchor='e').pack(side=tk.LEFT)
    entry_wo = tk.Entry(row1, width=20)
    entry_wo.pack(side=tk.LEFT, padx=5)

    def on_new_click():
        msg, rework_no = module_oracle.get_rework_no()
        if msg == "OK":
            entry_wo.delete(0, tk.END)
            entry_wo.insert(0, rework_no)
        else:
            messagebox.showerror("错误", f"生成重工号失败: {msg}")

    btn_new = tk.Button(row1, text="新增", command=on_new_click)
    btn_new.pack(side=tk.LEFT, padx=5)
    # 输入类型选择 创建下拉框时，从字典键生成选项列表
    row_type = tk.Frame(input_panel)
    row_type.pack(fill=tk.X, pady=2)
    tk.Label(row_type, text="类型选择：", width=10, anchor='e').pack(side=tk.LEFT)
    combo_type = ttk.Combobox(row_type, values=list(column_map.keys()), width=12)
    combo_type.current(0)  # 默认选择第一个
    combo_type.pack(side=tk.LEFT, padx=5)

    # 输入框
    row_input = tk.Frame(input_panel)
    row_input.pack(fill=tk.X, pady=2)
    tk.Label(row_input, text="输入：", width=10, anchor='e').pack(side=tk.LEFT)
    entry_input = tk.Entry(row_input, width=25)
    entry_input.pack(side=tk.LEFT, padx=5)

    def on_input_execute(event=None):
        input_type = combo_type.get()
        input_value = entry_input.get().strip()
        if not input_value:
            return
        
        if input_type in column_map:
            if '-' in input_value :
                if column_map[input_type] == "CARTON_NO" or column_map[input_type]  == "SERIAL_NUMBER":
                    prefix, start_num, end_num, width = parse_range_string(input_value)
                    valid_values = []
                    failed_values = []
                    for num in range(start_num, end_num + 1):
                        val = prefix + str(num).zfill(width)
                        check_result = module_oracle.check_exists_in_status(column_map, input_type, val)
                        if check_result != "OK":
                            failed_values.append(val)
                        else:
                            valid_values.append(val)
                    if failed_values:
                        messagebox.showerror("检查失败", f"以下值不存在: {', '.join(failed_values)}")
                        return
                    # 全部通过，添加
                    if input_type not in query_data:
                        query_data[input_type] = []
                    query_data[input_type].extend(valid_values)
                else:
                    messagebox.showerror("检查失败", "输入不合规")
                    return  # 停止后续操作
            else:
                check_result = module_oracle.check_exists_in_status(column_map,input_type, input_value)
                if check_result != "OK":
                    messagebox.showerror("检查失败", check_result)
                    return  # 停止后续操作
                if input_type not in query_data:
                    query_data[input_type] = []
                query_data[input_type].append(input_value)

        msg, cols, rows = module_oracle.fetch_rework_sn(query_data,column_map)
        if msg == "OK":
            route_name = check_route_uniqueness(cols,rows)
            if route_name:
                entry_route.delete(0, tk.END)
                entry_route.insert(0, route_name)
                on_route_enter(None)
            update_result_table(rows)
        entry_input.delete(0, tk.END)
        update_display()

    entry_input.bind('<Return>', on_input_execute)

    row_qty = tk.Frame(input_panel)
    row_qty.pack(fill=tk.X, pady=2)
    tk.Label(row_qty, text="数量：", width=10, anchor='e').pack(side=tk.LEFT)
    entry_qty = tk.Entry(row_qty, width=8, fg='red')   # 文字颜色红色
    entry_qty.insert(0, "0")
    entry_qty.config(state='readonly')   # 只读
    entry_qty.pack(side=tk.LEFT, padx=5)

    # 在数量行之后，添加输入条件表格
    frame_cond_display = tk.LabelFrame(input_panel, text="已输入查询条件", padx=5, pady=5)
    frame_cond_display.pack(fill=tk.BOTH, expand=True, pady=5)  # 让它可以扩展

    # 创建Treeview
    tree_cond = ttk.Treeview(frame_cond_display, columns=("类型", "值"), show="headings", height=5)
    tree_cond.heading("类型", text="类型")
    tree_cond.heading("值", text="值")
    tree_cond.column("类型", width=60, anchor="center")
    tree_cond.column("值", width=120, anchor="w")

    # 滚动条
    vsb_cond = ttk.Scrollbar(frame_cond_display, orient="vertical", command=tree_cond.yview)
    tree_cond.configure(yscrollcommand=vsb_cond.set)

    tree_cond.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    vsb_cond.pack(side=tk.RIGHT, fill=tk.Y)

    def update_display():
        """更新条件表格"""
        for item in tree_cond.get_children():
            tree_cond.delete(item)
        for typ, values in query_data.items():
            for val in values:
                tree_cond.insert("", tk.END, values=(typ, val))

    # 初始调用
    update_display()

    # 重工条件（复选框）
    frame_condition = tk.LabelFrame(input_panel, text="重工条件", padx=5, pady=5)
    frame_condition.pack(fill=tk.X, pady=10)
    conditions = [
        "清除出库序号",
        "包装（清除栈板、外箱、彩盒）",
        "抽验（清除抽验）",
        "清除MAC",
        "料件",
    ]
    check_vars = []
    for text in conditions:
        var = tk.BooleanVar()
        cb = tk.Checkbutton(frame_condition, text=text, variable=var)
        cb.pack(anchor='w', padx=5, pady=2)  # 左对齐，每个复选框占一行
        check_vars.append(var)

    # 重工流程：流程名称（输入框） + 投入制程（下拉选择）
    frame_route = tk.Frame(input_panel)
    frame_route.pack(fill=tk.X, pady=5)
    # 第一行：流程名称（输入框）
    row_route_name = tk.Frame(frame_route)
    row_route_name.pack(fill=tk.X, pady=2)
    tk.Label(row_route_name, text="流程名称", width=10, anchor='e').pack(side=tk.LEFT)
    entry_route = tk.Entry(row_route_name, width=18)
    entry_route.pack(side=tk.LEFT, padx=5)
    # 第二行：投入制程（下拉选择）
    row_route_process = tk.Frame(frame_route)
    row_route_process.pack(fill=tk.X, pady=2)
    tk.Label(row_route_process, text="投入制程", width=10, anchor='e').pack(side=tk.LEFT)
    combo_process = ttk.Combobox(row_route_process, values=[], width=18)
    combo_process.set('')  # 默认空
    combo_process.pack(side=tk.LEFT, padx=5)
    def on_route_enter(event):
        route_name = entry_route.get().strip()
        if route_name:
            try:
                columns, rows = module_oracle.fetch_route_steps_necessary(route_name)
                if columns and rows:
                    # 查找 PROCESS_NAME 列的索引
                    idx = None
                    for col in ["PROCESS_NAME"]:
                        if col in columns:
                            idx = columns.index(col)
                            break
                    if idx is not None:
                        processes = [row[idx] for row in rows]
                        # 去重并保留顺序
                        processes = list(dict.fromkeys(processes))
                        combo_process['values'] = processes
                        if processes:
                            combo_process.current(0)
                        else:
                            combo_process.set('')
                    else:
                        combo_process['values'] = []
                        combo_process.set('')
                else:
                    combo_process['values'] = []
                    combo_process.set('')
            except Exception as e:
                messagebox.showerror("查询错误", f"查询流程步骤失败：{e}")
                combo_process['values'] = []
                combo_process.set('')
        else:
            combo_process['values'] = []
            combo_process.set('')
    entry_route.bind('<Return>', on_route_enter)
    # 执行按钮
    def on_execute():
        """收集所有输入信息并打印/处理"""
        # 先获取各字段值
        rework_no = entry_wo.get().strip()
        qty_str = entry_qty.get().strip()
        route_name = entry_route.get().strip()
        process = combo_process.get().strip()
        is_new_wo = new_wo_var.get()
        new_wo_no = entry_new_wo.get().strip()
        # 校验必填项
        if not rework_no:
            messagebox.showwarning("提示", "重工单号不能为空")
            return
        if not qty_str or qty_str == "0":
            messagebox.showwarning("提示", "数量不能为0，请先查询有效数据")
            return
        if not route_name:
            messagebox.showwarning("提示", "流程名称不能为空")
            return
        if not process:
            messagebox.showwarning("提示", "投入制程不能为空")
            return
        # 若勾选了新工单，则新工单号不能为空
        if is_new_wo and not new_wo_no:
            messagebox.showwarning("提示", "已勾选新工单，请输入新工单号")
            return
        # 构建 info 字典
        info = {
            "重工单号": rework_no,
            "新工单": is_new_wo,
            "新工单号": new_wo_no,
            "数量": qty_str,
            "流程名称": route_name,
            "投入制程": process,
            "重工条件": [],
            "查询条件": query_data
        }
        # 收集复选框状态
        for idx, text in enumerate(conditions):
            if check_vars[idx].get():
                info["重工条件"].append(text)
        if show_confirm_dialog(info):
            print("用户确认执行，开始处理...")
        else:
            print("用户取消操作")

    btn_execute = tk.Button(input_panel, text="执行", bg="#4CAF50", fg="white", width=10, command=on_execute)
    btn_execute.pack(pady=20)

    # ----- 右侧面板：表格区域 -----
    right_frame = tk.Frame(paned)
    paned.add(right_frame, weight=3)

    table_frame = tk.Frame(right_frame)
    table_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

    columns = [
        "序号", "工单", "料号", "生产线", "WIP 制程", "制程类别", "工作站", "出货序号",
        "栈板号", "箱线", "彩盒", "生产线产出时间", "流程名称"
    ]
    tree = ttk.Treeview(table_frame, columns=columns, show="headings", height=15)
    for col in columns:
        tree.heading(col, text=col)
        tree.column(col, width=80, anchor="center", minwidth=60)

    vsb = ttk.Scrollbar(table_frame, orient="vertical", command=tree.yview)
    hsb = ttk.Scrollbar(table_frame, orient="horizontal", command=tree.xview)
    tree.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)

    tree.grid(row=0, column=0, sticky="nsew")
    vsb.grid(row=0, column=1, sticky="ns")
    hsb.grid(row=1, column=0, sticky="ew")

    table_frame.grid_rowconfigure(0, weight=1)
    table_frame.grid_columnconfigure(0, weight=1)

    global result_tree
    result_tree = tree

    def update_result_table(data):
        for item in result_tree.get_children():
            result_tree.delete(item)
        for row in data:
            result_tree.insert("", tk.END, values=row)
        entry_qty.config(state='normal')
        entry_qty.delete(0, tk.END)
        entry_qty.insert(0, str(len(data)))
        entry_qty.config(state='readonly')
    def show_confirm_dialog(info):
        """
        显示一个确认窗口，展示所有输入信息，并返回 True（确定）或 False（取消）
        :param info: 包含所有输入信息的字典
        :return: bool
        """
        win = tk.Toplevel(root)
        win.title("确认执行")
        win.geometry("500x400")
        win.transient(root)
        win.grab_set()
        # 格式化信息
        lines = []
        for key, value in info.items():
            if key == "查询条件":
                lines.append(f"\n{key}:")
                if isinstance(value, dict):
                    for k, v in value.items():
                        lines.append(f"  {k}: {', '.join(v)}")
                else:
                    lines.append(f"  {value}")
            elif key == "重工条件":
                if value:
                    lines.append(f"\n{key}: {', '.join(value)}")
                else:
                    lines.append(f"\n{key}: (无)")
            elif isinstance(value, bool):
                lines.append(f"{key}: {'是' if value else '否'}")
            else:
                lines.append(f"{key}: {value}")
        text = tk.Text(win, wrap=tk.WORD, padx=10, pady=10, height=15)
        text.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        text.insert(tk.END, "\n".join(lines))
        text.config(state=tk.DISABLED)
        # 按钮框架
        btn_frame = tk.Frame(win)
        btn_frame.pack(pady=10)
        result = False
        def on_ok():
            nonlocal result
            result = True
            win.destroy()
        def on_cancel():
            nonlocal result
            result = False
            win.destroy()
        btn_ok = tk.Button(btn_frame, text="确定", command=on_ok, width=10, bg="#4CAF50", fg="white")
        btn_ok.pack(side=tk.LEFT, padx=10)
        btn_cancel = tk.Button(btn_frame, text="取消", command=on_cancel, width=10)
        btn_cancel.pack(side=tk.LEFT, padx=10)
        win.wait_window()
        return result
# ---------- 主函数 -----------
def main():
    global root, content_frame, status_label, user_label,menubar

    root = tk.Tk()
    root.title("VarlikeTools")
    root.geometry("960x540")
    icon_path = resource_path("favicon.ico")
    root.iconbitmap(icon_path)
    
    module_thread.init(root)
    #加载版本号
    module_save.save_app_version(module_version.VERSION)
    # 加载窗口配置
    config = module_save.load_config()
    win_cfg = config['settings']['window']
    root.geometry(f"{win_cfg['width']}x{win_cfg['height']}+{win_cfg['x']}+{win_cfg['y']}")
    if win_cfg['maximized']:
        root.state('zoomed')

    # 尝试自动登录
    auto_user = module_login.init_login_from_config()
    if auto_user:
        # 自动登录成功
        initial_welcome = f"欢迎回来，{auto_user}"
        user_display = auto_user
    else:
        initial_welcome = "欢迎使用 VarlikeTools\n请点击菜单栏「用户->登录」"
        user_display = "未登录"
    # 菜单栏
    menubar = tk.Menu(root)
    root.config(menu=menubar)
    build_menu()
    # 内容区域
    content_frame = tk.Frame(root)
    content_frame.pack(fill=tk.BOTH, expand=True)
    welcome_label = tk.Label(content_frame, text=initial_welcome)
    welcome_label.pack(pady=20)
    # 底部
    bottom_frame = tk.Frame(root)
    bottom_frame.pack(side=tk.BOTTOM, fill=tk.X)
    status_label = tk.Label(bottom_frame, text="就绪", bd=1, relief=tk.SUNKEN, anchor=tk.W)
    status_label.pack(side=tk.TOP, fill=tk.X)
    user_label = tk.Label(bottom_frame, text=f"当前用户: {user_display}", bd=1, relief=tk.SUNKEN, anchor=tk.W)
    update_user_label(f"当前用户: {module_login.current_user}")
    user_label.pack(side=tk.TOP, fill=tk.X)
    # 加载配置
    config = module_save.load_config()
    refresh_interval = config['settings']['misc'].get('auto_refresh_interval', 0)
    auto_refresh_enabled = config['settings']['misc'].get('auto_refresh_enabled', False)
    if module_login.is_logined():
        # 初始化定时器，传入 root 和刷新函数（query_issue）
        module_timer.init_timer(root, lambda: query_issue(notify=True))
        # 如果启用，启动定时器
        if auto_refresh_enabled and refresh_interval > 0:
            module_timer.set_interval(refresh_interval)
            module_timer.start()
        else:
            # 确保停止
            module_timer.stop()
    else:
        config = module_save.load_config()
        config['settings']['misc']['auto_refresh_enabled'] = False
        module_save.save_config(config)
    # 窗口关闭保存配置
    def on_closing():
        module_timer.stop()
        config = module_save.load_config()
        if root.state() != 'zoomed':
            config['settings']['window']['width'] = root.winfo_width()
            config['settings']['window']['height'] = root.winfo_height()
            config['settings']['window']['x'] = root.winfo_x()
            config['settings']['window']['y'] = root.winfo_y()
        config['settings']['window']['maximized'] = (root.state() == 'zoomed')
        module_save.save_config(config)
        root.destroy()
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()
if __name__ == "__main__":
    main()