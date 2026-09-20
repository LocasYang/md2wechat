# MD2WeChat 使用示例

> 这是一份**示例文档**，用来展示 Markdown 转公众号排版后的效果。
> 左侧编辑，右侧实时预览，确认无误后点「复制到公众号」，到公众号编辑器里 `Ctrl+V` 粘贴即可。

## 一、为什么公众号排版这么麻烦

微信公众号的编辑器会把粘贴进来的 HTML **清洗一遍**：

| 会被丢弃 | 会被保留 |
| --- | --- |
| `<style>` 标签、`class` 属性 | 标签上的 `style="..."` 内联样式 |
| 外部 CSS、CSS 变量 | `<section>`、`<p>`、`<strong>` |
| `position: fixed`、伪元素 | 表格、图片、列表 |

所以本工具生成的文章 **全部使用内联样式**，不依赖任何外部资源。

### 1.1 行内元素

- **加粗**、*斜体*、***加粗斜体***、~~删除线~~
- 行内代码 `const theme = 'classic'`
- 链接写法：[Ant Design](https://ant.design)
- 裸链接也会自动识别：https://github.com/mengps/HuskarUI

#### 更小一级的标题

`h4` 及以下的标题会使用更细的左侧装饰条。

## 二、代码块

```cpp
// 代码块会按语言自动着色
#include <QApplication>

struct TocItem {
    QString title;
    int     level = 1;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    return app.exec();
}
```

```python
def read_markdown(path: str) -> str:
    """读取 Markdown 文件"""
    with open(path, "r", encoding="utf-8") as f:
        return f.read()

if __name__ == "__main__":
    text = read_markdown("demo.md")
    print(f"共 {len(text)} 个字符")
```

```json
{
  "name": "md2wechat",
  "primary": "#1677ff",
  "radius": 6,
  "themes": ["classic", "wechat", "minimal", "warm", "night"]
}
```

## 三、列表

1. 有序列表第一项
2. 有序列表第二项
   - 嵌套的无序项
   - 再来一条
3. 有序列表第三项

- [x] 已完成：Markdown 解析
- [x] 已完成：内联样式渲染
- [ ] 待办：图片自动上传图床
- [ ] 待办：一键同步到草稿箱

## 四、引用

> 好的排版不是让读者注意到排版本身，
> 而是让读者专注于内容。
>
> —— 某位排版强迫症患者

嵌套引用也能处理：

> 外层引用
> > 内层引用，会有另一层缩进和颜色

---

## 五、表格

| 主题 | 主色 | 标题装饰 | 适用场景 |
| :--- | :---: | :--- | ---: |
| 经典蓝 | `#1677ff` | 左侧色条 | 技术文章、产品说明 |
| 微信绿 | `#07c160` | 底色块 | 品牌宣传 |
| 极简黑白 | `#262626` | 居中下划线 | 深度长文 |
| 暖橙 | `#ff7a45` | 左条+浅底 | 生活、美食 |
| 夜读 | `#4d9fff` | 左侧色条 | 夜间阅读 |

## 六、图片与图注

Markdown 中的图片会被渲染为宽度自适应：

![](qrc:/examples/demo.png)

::: hljs-center
图注用 `::: hljs-center` 包起来，会自动居中，并使用小一号的浅色字
:::

支持三种对齐容器：`::: hljs-center`（居中）、`::: hljs-right`（右对齐）、`::: hljs-left`（左对齐）。

> 注：图片 alt 如果是文件名（比如 `![image.png](...)`），不会当作图注显示出来，
> 避免正文里冒出一句 "image.png"。

## 七、中文首行缩进

$\qquad$段落以 `$\qquad$` 或 `$~$` 开头时，会自动转换为首行缩进两字符，
符合中文排版习惯，不需要手工敲全角空格。

$\qquad$缩进标记出现在段落中间时，会当作普通空白处理，不会变成奇怪的图片。

## 八、数学公式

微信公众号**不支持 MathJax / MathML**，公式必须是图片。所以这里用本地 MathJax
把 LaTeX 离线渲染成 3 倍超采样的 PNG，再以内联样式嵌进正文 —— 不依赖任何外部服务，
断网也能用。

行内公式会和正文基线自动对齐，例如死区范围按 $|V_{grid}| < 15V$ 设定。

块级公式居中独立显示：

$$
|V_{grid}| > 15V \ \&\& \ |V_{grid}| < 30V
$$

复杂公式同样支持求和、分式、矩阵、希腊字母：

$$
\frac{\partial f(x)}{\partial v_i}=\sum_{j=1}^{n}\frac{\partial f(x)}{\partial u_j}\cdot\frac{\partial u_j}{\partial v_i}
$$

$$
Z_{th} = \begin{bmatrix} R_1 + j\omega L_1 & j\omega M \\ j\omega M & R_2 + j\omega L_2 \end{bmatrix}
$$

## 九、下一步

- 支持自定义主题变量（字号 / 主色 / 圆角）
- 支持图床（腾讯云 COS、又拍云、GitHub）
- 支持导出到公众号草稿箱
- 支持大纲导航与字数统计

> 提示：点击左侧「排版样式」可以随时切换 5 套预设，预览会立即更新。
