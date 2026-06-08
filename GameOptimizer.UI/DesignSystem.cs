using System.Drawing;
using System.Windows.Forms;

namespace GameOptimizer.UI;

public static class DesignSystem
{
    public static readonly Color BgColor = Color.FromArgb(248, 250, 252);
    public static readonly Color SurfaceColor = Color.White;
    public static readonly Color BorderColor = Color.FromArgb(229, 231, 235);

    public static readonly Color PrimaryColor = Color.FromArgb(37, 99, 235);
    public static readonly Color PrimaryHoverColor = Color.FromArgb(29, 78, 216);
    public static readonly Color SecondaryButtonColor = Color.WhiteSmoke;

    public static readonly Color TextTitle = Color.FromArgb(30, 41, 59);
    public static readonly Color TextBody = Color.FromArgb(51, 65, 85);
    public static readonly Color TextMuted = Color.FromArgb(100, 116, 139);

    public static readonly Color Success = Color.SeaGreen;
    public static readonly Color Warning = Color.DarkGoldenrod;
    public static readonly Color Danger = Color.Firebrick;

    private const string MainFontFamily = "Malgun Gothic";
    public static readonly Font FontTitle = new("Segoe UI", 18F, FontStyle.Bold);
    public static readonly Font FontHeading = new(MainFontFamily, 11F, FontStyle.Bold);
    public static readonly Font FontBody = new(MainFontFamily, 9.75F, FontStyle.Regular);
    public static readonly Font FontSmall = new(MainFontFamily, 8.5F, FontStyle.Regular);

    public static readonly Padding CardPadding = new(20);
    public const int ControlMargin = 12;
}
