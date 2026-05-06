在 Visual Studio 中打开‌：

​    关闭当前解决方案。
​    使用 ‌“文件” -> “打开” -> “文件夹”‌ 打开项目根目录。
​    Visual Studio 会自动检测到 CMakePresets.json。
​    在顶部工具栏的配置下拉菜单中，你现在应该能看到 ‌x64-debug‌ 和 ‌Win32 (x86) Debug‌ 两个选项。
​    选择 Win32 (x86) Debug，VS 会自动运行 CMake 配置，生成正确的 x86 构建环境。

使用对应的批处理可以生成VS解决方案