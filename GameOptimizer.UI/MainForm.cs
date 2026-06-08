using System.Diagnostics;
using System.Text;

namespace GameOptimizer.UI;

public sealed partial class MainForm : Form
{
    private const int FormWidth = 1180;

    private readonly ComboBox targetCombo = new();
    private readonly Button refreshButton = new();
    private readonly RadioButton dryRunRadio = new();
    private readonly RadioButton softApplyRadio = new();
    private readonly RadioButton applyRadio = new();
    private readonly CheckBox threadDetailCheck = new();
    private readonly CheckBox backgroundDetailCheck = new();
    private readonly CheckBox latencyPingCheck = new();
    private readonly TextBox latencyPingText = new();
    private readonly CheckBox backgroundFilterCheck = new();
    private readonly TextBox backgroundFilterText = new();
    private readonly Button browseFilterButton = new();
    private readonly NumericUpDown runtimeSecondsBox = new();
    private readonly Button startButton = new();
    private readonly Button evidenceFolderButton = new();
    private readonly Button latestReportButton = new();
    private readonly Button settingsToggleButton = new();
    private readonly Button detailsToggleButton = new();
    private readonly CheckBox runtimeLimitCheck = new();
    private readonly Label gameStateValue = new();
    private readonly Label statusValue = new();
    private readonly Label optimizeStateValue = new();
    private readonly Label recoveryStateValue = new();
    private readonly Label enginePathValue = new();
    private readonly Label modeDescriptionValue = new();
    private readonly Label runtimeDescriptionValue = new();
    private readonly TableLayoutPanel contentPanel = new();
    private readonly TableLayoutPanel settingsPanel = new();
    private readonly TableLayoutPanel detailsPanel = new();
    private readonly LineMetricControl cpuTrend = new() { Title = "CPU" };
    private readonly LineMetricControl networkTrend = new() { Title = "네트워크" };
    private readonly DonutMetricControl threadGauge = new() { Title = "스레드", Value = 72 };
    private readonly DonutMetricControl safetyGauge = new() { Title = "안전 복구", Value = 92 };
    private readonly ScoreGaugeControl healthGauge = new() { Value = 68, Caption = "최적화 점수" };
    private readonly ProgressMetricControl cpuMetric = new() { Title = "CPU 안정성", Detail = "메인 스레드 관찰 대기", Percent = 55 };
    private readonly ProgressMetricControl threadMetric = new() { Title = "스레드", Detail = "게임 스레드 감지 대기", Percent = 72 };
    private readonly ProgressMetricControl networkMetric = new() { Title = "네트워크", Detail = "RTT 관찰 대기", Percent = 70 };
    private readonly Label visualSummaryValue = new();
    private readonly Label heroTitleValue = new();
    private readonly Label heroDescriptionValue = new();
    private readonly List<string> hiddenLogLines = new();

    private Process? runningProcess;
    private bool settingsExpanded;
    private bool detailsExpanded;
    private int warningCount;
    private int blockerCount;
    private int logSampleCount;
    private double cpuScore = 55;
    private double networkScore = 70;
    private double threadScore = 72;
    private double safetyScore = 92;

    private sealed class CardPanel : Panel
    {
        public CardPanel()
        {
            DoubleBuffered = true;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            using var pen = new Pen(DesignSystem.BorderColor);
            var rectangle = ClientRectangle;
            rectangle.Width -= 1;
            rectangle.Height -= 1;
            e.Graphics.DrawRectangle(pen, rectangle);
        }
    }

    private sealed class LineMetricControl : Control
    {
        private readonly Queue<double> samples = new();

        public string Title { get; init; } = string.Empty;

        public LineMetricControl()
        {
            DoubleBuffered = true;
            Height = 112;
            MinimumSize = new Size(180, 112);
        }

        public void Push(double value)
        {
            samples.Enqueue(Math.Clamp(value, 0, 100));
            while (samples.Count > 36)
            {
                samples.Dequeue();
            }
            Invalidate();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            e.Graphics.Clear(DesignSystem.SurfaceColor);
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

            using var titleBrush = new SolidBrush(DesignSystem.TextTitle);
            using var mutedBrush = new SolidBrush(DesignSystem.TextMuted);
            using var gridPen = new Pen(DesignSystem.BorderColor);
            using var linePen = new Pen(DesignSystem.PrimaryColor, 2F);

            e.Graphics.DrawString(Title, DesignSystem.FontHeading, titleBrush, 0, 0);

            var plot = new Rectangle(0, 30, Math.Max(1, Width - 4), Math.Max(1, Height - 42));
            e.Graphics.DrawRectangle(gridPen, plot);
            e.Graphics.DrawString("효율", DesignSystem.FontSmall, mutedBrush, 0, Height - 16);

            if (samples.Count < 2)
            {
                e.Graphics.DrawString("대기 중", DesignSystem.FontSmall, mutedBrush, plot.Left + 8, plot.Top + 10);
                return;
            }

            var values = samples.ToArray();
            var points = new PointF[values.Length];
            for (var i = 0; i < values.Length; ++i)
            {
                var x = plot.Left + (plot.Width * i / Math.Max(1, values.Length - 1));
                var y = plot.Bottom - (plot.Height * (float)values[i] / 100F);
                points[i] = new PointF(x, y);
            }
            e.Graphics.DrawLines(linePen, points);

            var latest = values[^1];
            e.Graphics.DrawString($"{latest:0}%", DesignSystem.FontSmall, titleBrush, plot.Right - 36, plot.Top + 4);
        }
    }

    private sealed class DonutMetricControl : Control
    {
        public string Title { get; init; } = string.Empty;
        public double Value { get; set; }

        public DonutMetricControl()
        {
            DoubleBuffered = true;
            Height = 112;
            MinimumSize = new Size(132, 112);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            e.Graphics.Clear(DesignSystem.SurfaceColor);
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

            using var titleBrush = new SolidBrush(DesignSystem.TextTitle);
            using var mutedPen = new Pen(DesignSystem.BorderColor, 8F);
            using var valuePen = new Pen(Value >= 80 ? DesignSystem.Success : Value >= 55 ? DesignSystem.PrimaryColor : DesignSystem.Warning, 8F);
            using var textBrush = new SolidBrush(DesignSystem.TextBody);

            e.Graphics.DrawString(Title, DesignSystem.FontHeading, titleBrush, 0, 0);
            var size = Math.Min(Width - 12, Height - 34);
            var rect = new Rectangle((Width - size) / 2, 30, size, size);
            e.Graphics.DrawArc(mutedPen, rect, -90, 360);
            e.Graphics.DrawArc(valuePen, rect, -90, (float)Math.Clamp(Value, 0, 100) * 3.6F);

            var text = $"{Value:0}%";
            var textSize = e.Graphics.MeasureString(text, DesignSystem.FontHeading);
            e.Graphics.DrawString(text, DesignSystem.FontHeading, textBrush, rect.Left + (rect.Width - textSize.Width) / 2, rect.Top + (rect.Height - textSize.Height) / 2);
        }
    }

    private sealed class ScoreGaugeControl : Control
    {
        public double Value { get; set; }
        public string Caption { get; init; } = string.Empty;

        public ScoreGaugeControl()
        {
            DoubleBuffered = true;
            Size = new Size(160, 160);
            MinimumSize = new Size(150, 150);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            e.Graphics.Clear(DesignSystem.SurfaceColor);
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

            var size = Math.Min(Width, Height) - 16;
            var rect = new Rectangle((Width - size) / 2, 8, size, size);
            using var basePen = new Pen(Color.FromArgb(241, 245, 249), 10F);
            using var valuePen = new Pen(Value >= 85 ? DesignSystem.Success : Value >= 60 ? DesignSystem.Warning : DesignSystem.Danger, 10F);
            using var titleBrush = new SolidBrush(DesignSystem.TextTitle);
            using var mutedBrush = new SolidBrush(DesignSystem.TextMuted);

            e.Graphics.DrawArc(basePen, rect, -90, 360);
            e.Graphics.DrawArc(valuePen, rect, -90, (float)Math.Clamp(Value, 0, 100) * 3.6F);

            var scoreText = $"{Value:0}";
            using var scoreFont = new Font("Segoe UI", 34F, FontStyle.Bold);
            var scoreSize = e.Graphics.MeasureString(scoreText, scoreFont);
            e.Graphics.DrawString(scoreText, scoreFont, titleBrush, rect.Left + (rect.Width - scoreSize.Width) / 2, rect.Top + 38);

            var captionSize = e.Graphics.MeasureString(Caption, DesignSystem.FontSmall);
            e.Graphics.DrawString(Caption, DesignSystem.FontSmall, mutedBrush, rect.Left + (rect.Width - captionSize.Width) / 2, rect.Top + 92);
        }
    }

    private sealed class ProgressMetricControl : Control
    {
        public string Title { get; init; } = string.Empty;
        public string Detail { get; set; } = string.Empty;
        public double Percent { get; set; }

        public ProgressMetricControl()
        {
            DoubleBuffered = true;
            Height = 136;
            MinimumSize = new Size(210, 136);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            e.Graphics.Clear(DesignSystem.SurfaceColor);
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

            using var titleBrush = new SolidBrush(DesignSystem.TextTitle);
            using var textBrush = new SolidBrush(DesignSystem.TextBody);
            using var mutedBrush = new SolidBrush(DesignSystem.TextMuted);
            using var borderPen = new Pen(DesignSystem.BorderColor);

            var bounds = new Rectangle(0, 0, Width - 1, Height - 1);
            e.Graphics.DrawRectangle(borderPen, bounds);
            e.Graphics.DrawString(Title, DesignSystem.FontHeading, titleBrush, 18, 18);

            var percentText = $"{Percent:0}%";
            using var percentFont = new Font("Segoe UI", 20F, FontStyle.Bold);
            var percentSize = e.Graphics.MeasureString(percentText, percentFont);
            e.Graphics.DrawString(percentText, percentFont, titleBrush, Width - percentSize.Width - 18, 14);

            var barRect = new Rectangle(18, 68, Math.Max(1, Width - 36), 10);
            using var barBack = new SolidBrush(Color.FromArgb(241, 245, 249));
            using var barFore = new SolidBrush(Percent >= 80 ? DesignSystem.Success : Percent >= 55 ? DesignSystem.PrimaryColor : DesignSystem.Warning);
            e.Graphics.FillRectangle(barBack, barRect);
            e.Graphics.FillRectangle(barFore, new Rectangle(barRect.X, barRect.Y, (int)(barRect.Width * Math.Clamp(Percent, 0, 100) / 100), barRect.Height));

            e.Graphics.DrawString(Detail, DesignSystem.FontSmall, mutedBrush, 18, 90);
        }
    }

    private sealed class ProcessListItem
    {
        public required string ExeName { get; init; }
        public required int ProcessId { get; init; }
        public string WindowTitle { get; init; } = string.Empty;

        public override string ToString()
        {
            return string.IsNullOrWhiteSpace(WindowTitle)
                ? $"{ExeName}  (PID {ProcessId})"
                : $"{ExeName}  (PID {ProcessId}) - {WindowTitle}";
        }
    }

    private sealed class TargetSelection
    {
        public required string ExeName { get; init; }
        public int? ProcessId { get; init; }
    }

    public MainForm()
    {
        InitializeComponent();
        RefreshProcessList();
        UpdateEnginePathLabel();
        UpdateSummaryState("대기", DesignSystem.TextMuted, "최적화 대기 중");
        UpdateModeDescription();
        UpdateRuntimeLimitState();
        UpdateControlState(false);
        AdjustFormHeight();
    }

    private void BuildLayout()
    {
        BackColor = DesignSystem.BgColor;

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            BackColor = DesignSystem.BgColor,
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 250));
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        Controls.Add(root);

        root.Controls.Add(CreateSidebar(), 0, 0);

        var main = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            BackColor = DesignSystem.BgColor,
        };
        main.RowStyles.Add(new RowStyle(SizeType.Absolute, 64));
        main.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.Controls.Add(main, 1, 0);

        main.Controls.Add(CreateHeader(), 0, 0);

        var scrollHost = new Panel
        {
            Dock = DockStyle.Fill,
            AutoScroll = true,
            Padding = new Padding(28),
            BackColor = DesignSystem.BgColor,
        };
        main.Controls.Add(scrollHost, 0, 1);

        contentPanel.Dock = DockStyle.Top;
        contentPanel.AutoSize = true;
        contentPanel.ColumnCount = 1;
        contentPanel.RowCount = 5;
        contentPanel.BackColor = DesignSystem.BgColor;
        contentPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        contentPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        scrollHost.Controls.Add(contentPanel);

        contentPanel.Controls.Add(CreateSummaryPanel(), 0, 0);
        contentPanel.Controls.Add(CreateMetricGridPanel(), 0, 1);
        contentPanel.Controls.Add(CreateRecommendationPanel(), 0, 2);
        contentPanel.Controls.Add(CreateVisualMetricsPanel(), 0, 3);

        settingsPanel.Dock = DockStyle.Top;
        settingsPanel.AutoSize = true;
        settingsPanel.ColumnCount = 1;
        settingsPanel.RowCount = 3;
        settingsPanel.BackColor = DesignSystem.BgColor;
        settingsPanel.Visible = false;
        settingsPanel.Controls.Add(CreateEngineOptionsPanel(), 0, 0);
        settingsPanel.Controls.Add(CreateDetailsToggle(), 0, 1);

        detailsPanel.Dock = DockStyle.Top;
        detailsPanel.AutoSize = true;
        detailsPanel.ColumnCount = 1;
        detailsPanel.RowCount = 4;
        detailsPanel.Visible = false;
        detailsPanel.Controls.Add(CreateDetailSection(
            "CPU",
            new[] { "사용 코어 : 4 / 보조 코어 : 6", "CPU 상태 : 정상" },
            new[] { "Processor Group : 0", "Primary Core : 4", "Fallback Core : 6", "SMT 분리 : 적용됨", "CCX 최적화 : 적용됨" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "스레드",
            new[] { "게임 메인 스레드 감지됨 / 상태 : 안정적", "불필요한 이동 없음" },
            new[] { "메인 스레드 ID : 1234", "EMA 점수 : 91", "Stickiness : 4초", "Migration : 0회" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "네트워크",
            new[] { "네트워크 상태 : 좋음", "지연 변동 : 낮음", "간섭 신호 : 없음" },
            new[] { "RTT : 5ms", "RTT 변동 : 1.2ms", "DPC 발생 : 0회", "IRQ 재배치 : 없음" }));
        detailsPanel.Controls.Add(CreateDetailSection(
            "안전 복구",
            new[] { "복구 정보 저장 완료", "자동 복구 가능", "마지막 검사 : 정상" },
            new[] { "Affinity 백업 : 완료", "Priority 백업 : 완료", "ApplyGuard : 정상", "Rollback 준비 : 완료" }));
        settingsPanel.Controls.Add(detailsPanel, 0, 2);
        contentPanel.Controls.Add(settingsPanel, 0, 4);
    }

    private Control CreateSidebar()
    {
        var sidebar = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 6,
            BackColor = DesignSystem.SurfaceColor,
            Padding = new Padding(18),
        };
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        sidebar.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        sidebar.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        var brand = new Label
        {
            Text = "Game Optimizer",
            AutoSize = true,
            Font = DesignSystem.FontTitle,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 8, 0, 28),
        };
        sidebar.Controls.Add(brand, 0, 0);
        sidebar.Controls.Add(CreateNavButton("■  대시보드", active: true), 0, 1);
        sidebar.Controls.Add(CreateNavButton("~  성능 모니터", active: false), 0, 2);
        sidebar.Controls.Add(CreateNavButton("+  안전 복구", active: false), 0, 3);

        settingsToggleButton.Text = "설정";
        StyleButton(settingsToggleButton, primary: false, width: 190, height: 40);
        settingsToggleButton.Click += (_, _) => ToggleSettings();
        sidebar.Controls.Add(settingsToggleButton, 0, 5);
        return sidebar;
    }

    private static Button CreateNavButton(string text, bool active)
    {
        var button = new Button
        {
            Text = text,
            Width = 190,
            Height = 42,
            TextAlign = ContentAlignment.MiddleLeft,
            FlatStyle = FlatStyle.Flat,
            BackColor = active ? Color.FromArgb(239, 246, 255) : DesignSystem.SurfaceColor,
            ForeColor = active ? DesignSystem.PrimaryColor : DesignSystem.TextMuted,
            Font = active ? DesignSystem.FontHeading : DesignSystem.FontBody,
            Margin = new Padding(0, 0, 0, 8),
            Cursor = Cursors.Hand,
            UseVisualStyleBackColor = false,
        };
        button.FlatAppearance.BorderSize = 0;
        return button;
    }

    private Control CreateHeader()
    {
        var header = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            BackColor = Color.FromArgb(252, 253, 255),
            Padding = new Padding(28, 0, 28, 0),
        };
        header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        header.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        header.Controls.Add(new Label
        {
            Text = "시스템 개요",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Anchor = AnchorStyles.Left,
        }, 0, 0);
        header.Controls.Add(new Label
        {
            Text = "실시간 로그 기반",
            AutoSize = true,
            Font = DesignSystem.FontSmall,
            ForeColor = DesignSystem.TextMuted,
            Anchor = AnchorStyles.Right,
        }, 1, 0);
        return header;
    }

    private Control CreateSummaryPanel()
    {
        var panel = CreateCard();
        panel.Padding = new Padding(28);

        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, AutoSize = true, BackColor = DesignSystem.SurfaceColor };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 180));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        healthGauge.Dock = DockStyle.Fill;
        healthGauge.Margin = new Padding(0, 0, 30, 0);
        table.Controls.Add(healthGauge, 0, 0);

        var statusStack = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            ColumnCount = 1,
            RowCount = 3,
            BackColor = DesignSystem.SurfaceColor,
            Anchor = AnchorStyles.Left,
        };
        heroTitleValue.Text = "최적화가 권장됩니다";
        heroTitleValue.AutoSize = true;
        heroTitleValue.Font = new Font("Segoe UI", 20F, FontStyle.Bold);
        heroTitleValue.ForeColor = DesignSystem.TextTitle;
        heroTitleValue.Margin = new Padding(0, 14, 0, 10);
        statusStack.Controls.Add(heroTitleValue, 0, 0);
        heroDescriptionValue.Text = "게임 실행 상태와 런타임 로그를 기반으로 CPU, 스레드, 네트워크, 복구 안정성을 실시간으로 평가합니다.";
        heroDescriptionValue.AutoSize = true;
        heroDescriptionValue.MaximumSize = new Size(470, 0);
        heroDescriptionValue.Font = DesignSystem.FontBody;
        heroDescriptionValue.ForeColor = DesignSystem.TextMuted;
        statusStack.Controls.Add(heroDescriptionValue, 0, 1);
        table.Controls.Add(statusStack, 1, 0);

        var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Margin = new Padding(28, 22, 0, 0) };
        startButton.Text = "[최적화]";
        StyleButton(startButton, primary: true, width: 190, height: 48);
        startButton.Click += async (_, _) => await ToggleEngineAsync();
        actions.Controls.Add(startButton);
        table.Controls.Add(actions, 2, 0);

        return panel;
    }

    private Control CreateMetricGridPanel()
    {
        var panel = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 3,
            RowCount = 1,
            BackColor = DesignSystem.BgColor,
            Margin = new Padding(0, 0, 0, 24),
        };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.33F));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.33F));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.34F));
        cpuMetric.Dock = DockStyle.Fill;
        cpuMetric.Margin = new Padding(0, 0, 12, 0);
        threadMetric.Dock = DockStyle.Fill;
        threadMetric.Margin = new Padding(12, 0, 12, 0);
        networkMetric.Dock = DockStyle.Fill;
        networkMetric.Margin = new Padding(12, 0, 0, 0);
        panel.Controls.Add(cpuMetric, 0, 0);
        panel.Controls.Add(threadMetric, 1, 0);
        panel.Controls.Add(networkMetric, 2, 0);
        return panel;
    }

    private Control CreateRecommendationPanel()
    {
        var panel = CreateCard();
        panel.Padding = new Padding(20);

        var table = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 1, RowCount = 3, BackColor = DesignSystem.SurfaceColor };
        panel.Controls.Add(table);
        table.Controls.Add(new Label
        {
            Text = "권장 조치사항",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 0, 0, 12),
        }, 0, 0);
        table.Controls.Add(CreateRecommendationRow("!", "게임 프로세스 확인", "마비노기 실행 여부와 대상 PID를 확인합니다.", DesignSystem.Warning), 0, 1);
        table.Controls.Add(CreateRecommendationRow("~", "소프트 적용 유지", "시간 제한 없이 안정성을 관찰하며 최적화를 유지합니다.", DesignSystem.PrimaryColor), 0, 2);
        return panel;
    }

    private static Control CreateRecommendationRow(string badge, string title, string detail, Color accent)
    {
        var row = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 3,
            RowCount = 1,
            BackColor = DesignSystem.SurfaceColor,
            Padding = new Padding(12),
        };
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 46));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 28));

        var badgeLabel = new Label
        {
            Text = badge,
            Width = 32,
            Height = 32,
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.FromArgb(239, 246, 255),
            ForeColor = accent,
            Font = DesignSystem.FontHeading,
            Margin = new Padding(0, 0, 12, 0),
        };
        row.Controls.Add(badgeLabel, 0, 0);

        var textStack = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 1, RowCount = 2, BackColor = DesignSystem.SurfaceColor };
        textStack.Controls.Add(new Label { Text = title, AutoSize = true, Font = DesignSystem.FontHeading, ForeColor = DesignSystem.TextTitle }, 0, 0);
        textStack.Controls.Add(new Label { Text = detail, AutoSize = true, Font = DesignSystem.FontSmall, ForeColor = DesignSystem.TextMuted, Margin = new Padding(0, 4, 0, 0) }, 0, 1);
        row.Controls.Add(textStack, 1, 0);

        row.Controls.Add(new Label
        {
            Text = ">",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextMuted,
            Anchor = AnchorStyles.Right,
        }, 2, 0);
        return row;
    }

    private Control CreateStatusPanel()
    {
        var panel = CreateCard();
        panel.Padding = DesignSystem.CardPadding;

        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5, AutoSize = true, BackColor = DesignSystem.SurfaceColor };
        panel.Controls.Add(table);

        gameStateValue.Text = "마비노기 실행 중";
        gameStateValue.AutoSize = true;
        gameStateValue.Font = DesignSystem.FontHeading;
        gameStateValue.ForeColor = DesignSystem.PrimaryColor;
        gameStateValue.Margin = new Padding(0, 0, 0, 14);
        table.Controls.Add(gameStateValue);

        statusValue.Text = "현재 상태 : 대기";
        statusValue.AutoSize = true;
        statusValue.Font = DesignSystem.FontHeading;
        statusValue.ForeColor = DesignSystem.TextMuted;
        table.Controls.Add(statusValue);

        optimizeStateValue.Text = "최적화 적용 중";
        optimizeStateValue.AutoSize = true;
        optimizeStateValue.Font = DesignSystem.FontBody;
        optimizeStateValue.ForeColor = DesignSystem.TextBody;
        optimizeStateValue.Margin = new Padding(0, 8, 0, 0);
        table.Controls.Add(optimizeStateValue);

        recoveryStateValue.Text = "자동 복구 가능";
        recoveryStateValue.AutoSize = true;
        recoveryStateValue.Font = DesignSystem.FontSmall;
        recoveryStateValue.ForeColor = DesignSystem.TextMuted;
        recoveryStateValue.Margin = new Padding(0, 8, 0, 18);
        table.Controls.Add(recoveryStateValue);

        modeDescriptionValue.Text = "현재 모드는 게임 상태를 관찰하고 로그만 남깁니다. 시스템 설정은 바꾸지 않습니다.";
        modeDescriptionValue.AutoSize = true;
        modeDescriptionValue.MaximumSize = new Size(GetTextWrapWidth(), 0);
        modeDescriptionValue.Font = DesignSystem.FontSmall;
        modeDescriptionValue.ForeColor = DesignSystem.TextMuted;
        modeDescriptionValue.Margin = new Padding(0, 0, 0, 18);
        table.Controls.Add(modeDescriptionValue);

        return panel;
    }

    private Control CreateVisualMetricsPanel()
    {
        var panel = CreateCard();
        panel.Padding = DesignSystem.CardPadding;

        var table = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 2,
            RowCount = 4,
            BackColor = DesignSystem.SurfaceColor,
        };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        panel.Controls.Add(table);

        var heading = new Label
        {
            Text = "실시간 효율",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 0, 0, 8),
        };
        table.SetColumnSpan(heading, 2);
        table.Controls.Add(heading, 0, 0);

        visualSummaryValue.Text = "엔진 로그를 기다리는 중";
        visualSummaryValue.AutoSize = true;
        visualSummaryValue.MaximumSize = new Size(GetTextWrapWidth(), 0);
        visualSummaryValue.Font = DesignSystem.FontSmall;
        visualSummaryValue.ForeColor = DesignSystem.TextMuted;
        visualSummaryValue.Margin = new Padding(0, 0, 0, 10);
        table.SetColumnSpan(visualSummaryValue, 2);
        table.Controls.Add(visualSummaryValue, 0, 1);

        cpuTrend.Dock = DockStyle.Fill;
        cpuTrend.Margin = new Padding(0, 0, 8, 12);
        networkTrend.Dock = DockStyle.Fill;
        networkTrend.Margin = new Padding(8, 0, 0, 12);
        table.Controls.Add(cpuTrend, 0, 2);
        table.Controls.Add(networkTrend, 1, 2);

        threadGauge.Dock = DockStyle.Fill;
        threadGauge.Margin = new Padding(0, 0, 8, 0);
        safetyGauge.Dock = DockStyle.Fill;
        safetyGauge.Margin = new Padding(8, 0, 0, 0);
        table.Controls.Add(threadGauge, 0, 3);
        table.Controls.Add(safetyGauge, 1, 3);

        ResetVisualMetrics();
        return panel;
    }


    private Control CreateDetailsToggle()
    {
        var panel = CreateCard();
        panel.Padding = new Padding(16, 10, 16, 10);
        detailsToggleButton.Dock = DockStyle.Fill;
        detailsToggleButton.FlatStyle = FlatStyle.Flat;
        detailsToggleButton.FlatAppearance.BorderSize = 0;
        detailsToggleButton.Cursor = Cursors.Hand;
        detailsToggleButton.BackColor = DesignSystem.SurfaceColor;
        detailsToggleButton.ForeColor = DesignSystem.PrimaryColor;
        detailsToggleButton.Font = DesignSystem.FontHeading;
        detailsToggleButton.TextAlign = ContentAlignment.MiddleCenter;
        detailsToggleButton.Text = "▼ 모니터링 상세 정보 열기";
        detailsToggleButton.Height = 40;
        detailsToggleButton.Click += (_, _) => ToggleDetails();
        panel.Controls.Add(detailsToggleButton);
        return panel;
    }

    private Control CreateDetailSection(string title, string[] summaryLines, string[] advancedLines)
    {
        var panel = CreateCard();
        panel.Padding = DesignSystem.CardPadding;

        var table = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 2, RowCount = 3, AutoSize = true, BackColor = DesignSystem.SurfaceColor };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        var header = new Label
        {
            Text = $"▼ {title}",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Anchor = AnchorStyles.Left,
        };
        table.Controls.Add(header, 0, 0);

        var advancedButton = new Button
        {
            Text = "고급",
            Anchor = AnchorStyles.Right,
        };
        StyleButton(advancedButton, primary: false, width: 64, height: 30);
        table.Controls.Add(advancedButton, 1, 0);

        var summary = CreateTextStack(summaryLines, false);
        summary.Margin = new Padding(0, 10, 0, 0);
        table.SetColumnSpan(summary, 2);
        table.Controls.Add(summary, 0, 1);

        var advanced = CreateTextStack(advancedLines, true);
        advanced.Visible = false;
        advanced.Margin = new Padding(0, 12, 0, 0);
        table.SetColumnSpan(advanced, 2);
        table.Controls.Add(advanced, 0, 2);

        advancedButton.Click += (_, _) => advanced.Visible = !advanced.Visible;

        return panel;
    }

    private Control CreateTextStack(IEnumerable<string> lines, bool developerInfo)
    {
        var stack = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            ColumnCount = 1,
            AutoSize = true,
            BackColor = developerInfo ? DesignSystem.BgColor : DesignSystem.SurfaceColor,
        };
        if (developerInfo)
        {
            stack.Padding = new Padding(12);
        }

        foreach (var line in lines)
        {
            stack.Controls.Add(new Label
            {
                Text = line,
                AutoSize = true,
                ForeColor = developerInfo ? DesignSystem.PrimaryColor : DesignSystem.TextBody,
                Font = developerInfo ? new Font("Consolas", 9.5F) : DesignSystem.FontBody,
                Margin = new Padding(0, 2, 0, 4),
            });
        }

        return stack;
    }

    private Control CreateEngineOptionsPanel()
    {
        var panel = CreateCard();
        panel.Padding = DesignSystem.CardPadding;

        var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 16, AutoSize = true, BackColor = DesignSystem.SurfaceColor };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(table);

        var heading = new Label
        {
            Text = "엔진 설정",
            AutoSize = true,
            Font = DesignSystem.FontHeading,
            ForeColor = DesignSystem.TextTitle,
            Margin = new Padding(0, 0, 0, 10),
        };
        table.SetColumnSpan(heading, 3);
        table.Controls.Add(heading, 0, 0);

        var targetLabel = new Label { Text = "대상", AutoSize = true, Anchor = AnchorStyles.Left };
        targetLabel.Font = DesignSystem.FontBody;
        targetLabel.ForeColor = DesignSystem.TextBody;
        targetCombo.Dock = DockStyle.Fill;
        targetCombo.DropDownStyle = ComboBoxStyle.DropDown;
        targetCombo.FlatStyle = FlatStyle.Flat;
        StyleInput(targetCombo);
        refreshButton.Text = "새로고침";
        StyleButton(refreshButton, primary: false, width: 104, height: 30);
        refreshButton.Click += (_, _) => RefreshProcessList();
        table.Controls.Add(targetLabel, 0, 1);
        table.Controls.Add(targetCombo, 1, 1);
        table.Controls.Add(refreshButton, 2, 1);

        enginePathValue.Text = "엔진: 확인 중";
        enginePathValue.AutoSize = true;
        enginePathValue.Font = DesignSystem.FontSmall;
        enginePathValue.ForeColor = DesignSystem.TextMuted;
        enginePathValue.Margin = new Padding(0, 8, 0, 10);
        table.SetColumnSpan(enginePathValue, 3);
        table.Controls.Add(enginePathValue, 0, 2);

        var modeFlow = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, WrapContents = true };
        dryRunRadio.Text = "드라이런";
        softApplyRadio.Text = "소프트 적용";
        applyRadio.Text = "실제 적용";
        StyleOption(dryRunRadio);
        StyleOption(softApplyRadio);
        StyleOption(applyRadio);
        applyRadio.Checked = true;
        dryRunRadio.CheckedChanged += (_, _) => UpdateModeDescription();
        softApplyRadio.CheckedChanged += (_, _) => UpdateModeDescription();
        applyRadio.CheckedChanged += (_, _) => UpdateApplyWarning();
        modeFlow.Controls.AddRange(new Control[] { dryRunRadio, softApplyRadio, applyRadio });
        table.SetColumnSpan(modeFlow, 3);
        table.Controls.Add(modeFlow, 0, 3);

        AddHelpText(table, "기본값은 시간 제한 없이 유지되는 소프트 적용입니다. 드라이런은 관찰만 하고, 실제 적용은 확인창을 거칩니다.", 4);

        threadDetailCheck.Text = "스레드 상세 로깅";
        StyleOption(threadDetailCheck);
        table.SetColumnSpan(threadDetailCheck, 3);
        table.Controls.Add(threadDetailCheck, 0, 5);
        AddHelpText(table, "게임 메인 스레드 감지, 이동 여부, 안정성 판단 근거를 로그에 더 자세히 남깁니다.", 6);

        backgroundDetailCheck.Text = "백그라운드 상세 로깅";
        StyleOption(backgroundDetailCheck);
        table.SetColumnSpan(backgroundDetailCheck, 3);
        table.Controls.Add(backgroundDetailCheck, 0, 7);
        AddHelpText(table, "게임 외 프로세스가 최적화 판단에 영향을 주는지 확인하기 위한 진단 로그입니다.", 8);

        latencyPingCheck.Text = "지연시간 Ping";
        StyleOption(latencyPingCheck);
        latencyPingText.Text = "8.8.8.8";
        latencyPingText.Dock = DockStyle.Fill;
        latencyPingText.BorderStyle = BorderStyle.FixedSingle;
        StyleInput(latencyPingText);
        table.Controls.Add(latencyPingCheck, 0, 9);
        table.Controls.Add(latencyPingText, 1, 9);
        AddHelpText(table, "입력한 주소로 RTT 변동을 관찰합니다. 네트워크 설정을 직접 변경하지 않습니다.", 10);

        backgroundFilterCheck.Text = "백그라운드 필터";
        StyleOption(backgroundFilterCheck);
        backgroundFilterText.Dock = DockStyle.Fill;
        backgroundFilterText.BorderStyle = BorderStyle.FixedSingle;
        backgroundFilterText.Text = FindDefaultBackgroundFilter();
        StyleInput(backgroundFilterText);
        browseFilterButton.Text = "...";
        StyleButton(browseFilterButton, primary: false, width: 36, height: 28);
        browseFilterButton.Click += (_, _) => BrowseBackgroundFilter();
        table.Controls.Add(backgroundFilterCheck, 0, 11);
        table.Controls.Add(backgroundFilterText, 1, 11);
        table.Controls.Add(browseFilterButton, 2, 11);
        AddHelpText(table, "무시하거나 별도 판단할 백그라운드 프로세스 목록 파일입니다.", 12);

        runtimeLimitCheck.Text = "실행 시간 제한";
        StyleOption(runtimeLimitCheck);
        runtimeLimitCheck.CheckedChanged += (_, _) => UpdateRuntimeLimitState();
        runtimeSecondsBox.Minimum = 5;
        runtimeSecondsBox.Maximum = 3600;
        runtimeSecondsBox.Value = 60;
        runtimeSecondsBox.Increment = 5;
        runtimeSecondsBox.Dock = DockStyle.Left;
        runtimeSecondsBox.BackColor = DesignSystem.BgColor;
        runtimeSecondsBox.ForeColor = DesignSystem.TextBody;
        table.Controls.Add(runtimeLimitCheck, 0, 13);
        table.Controls.Add(runtimeSecondsBox, 1, 13);
        runtimeDescriptionValue.Text = "꺼두면 사용자가 종료를 누르거나 엔진이 종료될 때까지 계속 유지됩니다. 켜면 지정한 시간 뒤 종료됩니다.";
        runtimeDescriptionValue.AutoSize = true;
        runtimeDescriptionValue.MaximumSize = new Size(GetTextWrapWidth(), 0);
        runtimeDescriptionValue.Font = DesignSystem.FontSmall;
        runtimeDescriptionValue.ForeColor = DesignSystem.TextMuted;
        runtimeDescriptionValue.Margin = new Padding(0, 2, 0, 8);
        table.SetColumnSpan(runtimeDescriptionValue, 3);
        table.Controls.Add(runtimeDescriptionValue, 0, 14);

        var utilityFlow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = true, Margin = new Padding(0, 12, 0, 0) };
        evidenceFolderButton.Text = "로그 폴더";
        StyleButton(evidenceFolderButton, primary: false, width: 104);
        evidenceFolderButton.Click += (_, _) => OpenEvidenceFolder();
        latestReportButton.Text = "최근 리포트";
        StyleButton(latestReportButton, primary: false, width: 116);
        latestReportButton.Click += (_, _) => OpenLatestReport();
        utilityFlow.Controls.AddRange(new Control[] { evidenceFolderButton, latestReportButton });
        table.SetColumnSpan(utilityFlow, 3);
        table.Controls.Add(utilityFlow, 0, 15);

        return panel;
    }

    private static Panel CreateCard()
    {
        return new CardPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            BackColor = DesignSystem.SurfaceColor,
            Margin = new Padding(0, 0, 0, DesignSystem.ControlMargin),
        };
    }

    private static void AddHelpText(TableLayoutPanel table, string text, int row)
    {
        var helpText = CreateHelpText(text);
        table.SetColumnSpan(helpText, 3);
        table.Controls.Add(helpText, 0, row);
    }

    private static Label CreateHelpText(string text)
    {
        return new Label
        {
            Text = text,
            AutoSize = true,
            MaximumSize = new Size(GetTextWrapWidth(), 0),
            Font = DesignSystem.FontSmall,
            ForeColor = DesignSystem.TextMuted,
            Margin = new Padding(0, 0, 0, 8),
        };
    }

    private static int GetTextWrapWidth()
    {
        return FormWidth - (DesignSystem.CardPadding.Horizontal * 3);
    }

    private static void StyleButton(Button button, bool primary, int width, int height = 36)
    {
        button.Width = width;
        button.Height = height;
        button.FlatStyle = FlatStyle.Flat;
        button.FlatAppearance.BorderSize = primary ? 0 : 1;
        button.FlatAppearance.BorderColor = primary ? DesignSystem.PrimaryColor : DesignSystem.BorderColor;
        button.BackColor = primary ? DesignSystem.PrimaryColor : DesignSystem.SecondaryButtonColor;
        button.ForeColor = primary ? Color.White : DesignSystem.TextBody;
        button.Font = DesignSystem.FontBody;
        button.Cursor = Cursors.Hand;
        button.Margin = new Padding(0, 0, 8, 8);
        button.UseVisualStyleBackColor = false;
        button.MouseEnter += (_, _) =>
        {
            button.BackColor = primary ? DesignSystem.PrimaryHoverColor : DesignSystem.BorderColor;
            button.ForeColor = primary ? Color.White : DesignSystem.TextTitle;
        };
        button.MouseLeave += (_, _) =>
        {
            button.BackColor = primary ? DesignSystem.PrimaryColor : DesignSystem.SecondaryButtonColor;
            button.ForeColor = primary ? Color.White : DesignSystem.TextBody;
        };
    }

    private static void StyleInput(Control control)
    {
        control.BackColor = DesignSystem.SurfaceColor;
        control.ForeColor = DesignSystem.TextBody;
        control.Font = DesignSystem.FontBody;
        control.Margin = new Padding(0, 0, 8, 8);
    }

    private static void StyleOption(ButtonBase control)
    {
        control.BackColor = DesignSystem.SurfaceColor;
        control.ForeColor = DesignSystem.TextBody;
        control.Font = DesignSystem.FontBody;
        control.FlatStyle = FlatStyle.Flat;
        control.FlatAppearance.BorderSize = 0;
        control.Margin = new Padding(0, 0, 14, 8);
        control.UseVisualStyleBackColor = false;
    }

    private void ToggleDetails()
    {
        detailsExpanded = !detailsExpanded;
        detailsPanel.Visible = detailsExpanded;
        detailsToggleButton.Text = detailsExpanded ? "▲ 모니터링 상세 정보 닫기" : "▼ 모니터링 상세 정보 열기";
        AdjustFormHeight();
    }

    private void ToggleSettings()
    {
        settingsExpanded = !settingsExpanded;
        settingsPanel.Visible = settingsExpanded;
        settingsToggleButton.Text = settingsExpanded ? "설정 닫기" : "설정 열기";
        AdjustFormHeight();
    }

    private void AdjustFormHeight()
    {
        // Dashboard mode keeps a stable canvas; inner content scrolls when settings/details expand.
        var workingArea = Screen.FromControl(this).WorkingArea;
        Height = Math.Min(760, workingArea.Height - 40);
    }

    private void UpdateModeDescription()
    {
        if (dryRunRadio.Checked)
        {
            UpdatePrimaryButtonText();
            modeDescriptionValue.Text = "현재 모드는 관찰 전용입니다. 게임/윈도우 설정은 바꾸지 않고 적용 가능 여부와 위험 신호만 확인합니다.";
            if (runningProcess is null)
            {
                optimizeStateValue.Text = "상태 점검 대기 중";
            }
            return;
        }

        if (softApplyRadio.Checked)
        {
            UpdatePrimaryButtonText();
            modeDescriptionValue.Text = "기본 최적화 모드입니다. 시간 제한 없이 유지되며, 위험 신호가 있으면 적용하지 않거나 되돌릴 수 있게 동작합니다.";
            if (runningProcess is null)
            {
                optimizeStateValue.Text = "최적화 대기 중";
            }
            return;
        }

        UpdatePrimaryButtonText();
        modeDescriptionValue.Text = "현재 모드는 실제 적용입니다. 스레드 우선순위와 affinity 같은 실행 설정을 변경할 수 있습니다.";
        if (runningProcess is null)
        {
            optimizeStateValue.Text = "실제 적용 준비";
        }
    }

    private void UpdateRuntimeLimitState()
    {
        runtimeSecondsBox.Enabled = runtimeLimitCheck.Checked;
        runtimeDescriptionValue.Text = runtimeLimitCheck.Checked
            ? "지정한 시간이 지나면 엔진이 종료되고 복구 절차가 진행됩니다."
            : "시간 제한 없이 유지됩니다. 사용자가 종료를 누르거나 엔진이 종료될 때까지 계속 동작합니다.";
    }

    private void RefreshProcessList()
    {
        var previous = GetSelectedTargetName();
        targetCombo.Items.Clear();
        var items = Process.GetProcesses()
            .Select(CreateProcessListItem)
            .Where(item => item is not null)
            .Select(item => item!)
            .OrderByDescending(IsLikelyMabinogi)
            .ThenBy(item => item.ExeName, StringComparer.OrdinalIgnoreCase)
            .ThenBy(item => item.ProcessId)
            .ToList();

        foreach (var item in items)
        {
            targetCombo.Items.Add(item);
        }

        var previousMatch = items.FindIndex(item => string.Equals(item.ExeName, previous, StringComparison.OrdinalIgnoreCase));
        var mabinogiMatch = items.FindIndex(IsLikelyMabinogi);
        if (previousMatch >= 0)
        {
            targetCombo.SelectedIndex = previousMatch;
        }
        else if (mabinogiMatch >= 0)
        {
            targetCombo.SelectedIndex = mabinogiMatch;
        }
        else
        {
            targetCombo.SelectedIndex = -1;
            targetCombo.Text = string.Empty;
        }

        gameStateValue.Text = mabinogiMatch >= 0 ? "마비노기 실행 중" : "게임 대기 중";
    }

    private static bool IsLikelyMabinogi(ProcessListItem item)
    {
        return item.ExeName.Contains("mabinogi", StringComparison.OrdinalIgnoreCase) ||
               item.WindowTitle.Contains("mabinogi", StringComparison.OrdinalIgnoreCase) ||
               item.WindowTitle.Contains("마비노기", StringComparison.OrdinalIgnoreCase);
    }

    private static ProcessListItem? CreateProcessListItem(Process process)
    {
        try
        {
            var name = process.ProcessName;
            if (string.IsNullOrWhiteSpace(name))
            {
                return null;
            }

            return new ProcessListItem
            {
                ExeName = name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? name : $"{name}.exe",
                ProcessId = process.Id,
                WindowTitle = process.MainWindowTitle,
            };
        }
        catch
        {
            return null;
        }
    }

    private async Task ToggleEngineAsync()
    {
        if (runningProcess is not null)
        {
            StopEngine();
            return;
        }

        await StartEngineAsync();
    }

    private async Task StartEngineAsync()
    {
        if (runningProcess is not null)
        {
            return;
        }

        var target = GetSelectedTarget();
        if (string.IsNullOrWhiteSpace(target.ExeName))
        {
            MessageBox.Show("대상 프로세스를 선택하거나 입력하세요.", "대상 필요", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        var enginePath = FindEnginePath();
        if (enginePath is null)
        {
            MessageBox.Show("GameOptimizer.exe를 찾지 못했습니다. Release 빌드를 먼저 생성하세요.", "엔진 없음", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        ClearLog();
        UpdateSummaryState("세팅중", DesignSystem.PrimaryColor, "최적화 준비 중");
        visualSummaryValue.Text = "엔진 시작 중";
        UpdateControlState(true);

        var psi = new ProcessStartInfo
        {
            FileName = enginePath.FullName,
            WorkingDirectory = enginePath.DirectoryName ?? AppContext.BaseDirectory,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
        };
        var arguments = BuildArgumentList(target);
        foreach (var argument in arguments)
        {
            psi.ArgumentList.Add(argument);
        }

        var process = new Process { StartInfo = psi, EnableRaisingEvents = true };
        process.OutputDataReceived += (_, e) => AppendLogLine(e.Data);
        process.ErrorDataReceived += (_, e) => AppendLogLine(e.Data);
        process.Exited += (_, _) => BeginInvoke(new Action(() =>
        {
            var exitCode = process.ExitCode;
            process.Dispose();
            runningProcess = null;
            UpdateControlState(false);
            UpdateSummaryState("대기", exitCode == 0 ? DesignSystem.Success : DesignSystem.Danger, exitCode == 0 ? "최적화 완료" : "최적화 중단");
            visualSummaryValue.Text = exitCode == 0 ? "최종 상태 안정" : "최종 상태 점검 필요";
            AppendLogLine(exitCode == 0
                ? "[PASS] UI: 엔진 실행이 정상 종료되었습니다."
                : $"[BLOCKER] UI: 엔진 종료 코드 {exitCode}");
        }));

        try
        {
            runningProcess = process;
            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
            UpdateSummaryState("적용중", DesignSystem.Success, "최적화 적용 중");
            visualSummaryValue.Text = "실시간 로그 수집 중";
            AppendLogLine($"[INFO] UI: {enginePath.FullName}");
            AppendLogLine($"[INFO] UI: GameOptimizer.exe {FormatArgumentsForLog(arguments)}");
            await Task.CompletedTask;
        }
        catch (Exception ex)
        {
            runningProcess = null;
            process.Dispose();
            UpdateControlState(false);
            UpdateSummaryState("대기", DesignSystem.Danger, "최적화 실패");
            AppendLogLine($"[BLOCKER] UI: 실행 실패 - {ex.Message}");
        }
    }

    private List<string> BuildArgumentList(TargetSelection target)
    {
        var args = new List<string> { target.ExeName };
        if (target.ProcessId is { } processId)
        {
            args.Add("--pid");
            args.Add(processId.ToString());
        }
        if (dryRunRadio.Checked)
        {
            args.Add("--dry-run");
        }
        if (applyRadio.Checked)
        {
            args.Add("--apply");
        }
        if (threadDetailCheck.Checked)
        {
            args.Add("--thread-detail-log");
        }
        if (backgroundDetailCheck.Checked)
        {
            args.Add("--background-detail-log");
        }
        if (latencyPingCheck.Checked && !string.IsNullOrWhiteSpace(latencyPingText.Text))
        {
            args.Add("--latency-ping");
            args.Add(latencyPingText.Text.Trim());
        }
        if (backgroundFilterCheck.Checked && !string.IsNullOrWhiteSpace(backgroundFilterText.Text))
        {
            args.Add("--background-filter");
            args.Add(backgroundFilterText.Text.Trim());
        }
        if (runtimeLimitCheck.Checked)
        {
            args.Add("--max-runtime-seconds");
            args.Add(((int)runtimeSecondsBox.Value).ToString());
        }
        return args;
    }

    private string GetSelectedTargetName()
    {
        if (targetCombo.SelectedItem is ProcessListItem item)
        {
            return item.ExeName;
        }
        return targetCombo.Text.Trim();
    }

    private TargetSelection GetSelectedTarget()
    {
        if (targetCombo.SelectedItem is ProcessListItem item)
        {
            return new TargetSelection { ExeName = item.ExeName, ProcessId = item.ProcessId };
        }

        return new TargetSelection { ExeName = targetCombo.Text.Trim() };
    }

    private static string FormatArgumentsForLog(IEnumerable<string> arguments)
    {
        return string.Join(" ", arguments.Select(Quote));
    }

    private static string Quote(string value)
    {
        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }

    private void StopEngine()
    {
        if (runningProcess is null)
        {
            UpdateSummaryState("대기", DesignSystem.Success, "종료 완료");
            AppendLogLine("[INFO] UI: 실행 중인 엔진이 없어 상태만 복구 완료로 표시했습니다.");
            return;
        }

        try
        {
            runningProcess.Kill(entireProcessTree: true);
            UpdateSummaryState("종료중", DesignSystem.Warning, "종료 실행 중");
            AppendLogLine("[WARN] UI: 사용자가 종료를 요청했습니다.");
        }
        catch (Exception ex)
        {
            AppendLogLine($"[WARN] UI: 종료 실패 - {ex.Message}");
        }
    }

    private void AppendLogLine(string? line)
    {
        if (string.IsNullOrEmpty(line))
        {
            return;
        }

        if (InvokeRequired)
        {
            BeginInvoke(new Action(() => AppendLogLine(line)));
            return;
        }

        UpdateVisualMetrics(line);

        if (line.Contains("[BLOCKER]", StringComparison.OrdinalIgnoreCase) ||
            line.Contains("[ERROR]", StringComparison.OrdinalIgnoreCase) ||
            line.Contains("[FAIL]", StringComparison.OrdinalIgnoreCase))
        {
            statusValue.ForeColor = DesignSystem.Danger;
            optimizeStateValue.Text = "최적화 중단";
        }
        else if (line.Contains("[WARN]", StringComparison.OrdinalIgnoreCase))
        {
            if (statusValue.ForeColor != DesignSystem.Danger)
            {
                statusValue.ForeColor = DesignSystem.Warning;
            }
        }

        hiddenLogLines.Add(line);
    }

    private void ClearLog()
    {
        hiddenLogLines.Clear();
        ResetVisualMetrics();
    }

    private void ResetVisualMetrics()
    {
        warningCount = 0;
        blockerCount = 0;
        logSampleCount = 0;
        cpuScore = 55;
        networkScore = 70;
        threadScore = 72;
        safetyScore = 92;
        cpuTrend.Push(cpuScore);
        networkTrend.Push(networkScore);
        threadGauge.Value = threadScore;
        safetyGauge.Value = safetyScore;
        healthGauge.Value = 68;
        cpuMetric.Percent = cpuScore;
        threadMetric.Percent = threadScore;
        networkMetric.Percent = networkScore;
        cpuMetric.Detail = "엔진 로그 대기 중";
        threadMetric.Detail = "메인 스레드 대기 중";
        networkMetric.Detail = "RTT 관찰 대기 중";
        threadGauge.Invalidate();
        safetyGauge.Invalidate();
        healthGauge.Invalidate();
        cpuMetric.Invalidate();
        threadMetric.Invalidate();
        networkMetric.Invalidate();
        visualSummaryValue.Text = "엔진 로그를 기다리는 중";
    }

    private void UpdateVisualMetrics(string line)
    {
        ++logSampleCount;
        var lower = line.ToLowerInvariant();

        if (lower.Contains("warn"))
        {
            ++warningCount;
        }
        if (lower.Contains("blocker") || lower.Contains("error") || lower.Contains("fail"))
        {
            ++blockerCount;
        }

        var cpuMs = TryReadNumberAfter(line, "EMA-sample-cpu:");
        if (cpuMs is null)
        {
            cpuMs = TryReadNumberAfter(line, "avg-sample-cpu:");
        }
        if (cpuMs is { } cpuValue)
        {
            cpuScore = Smooth(cpuScore, Math.Clamp(100 - (cpuValue * 4), 10, 100));
        }
        else if (lower.Contains("cpu") || lower.Contains("thread"))
        {
            cpuScore = Smooth(cpuScore, blockerCount > 0 ? 35 : warningCount > 0 ? 65 : 82);
        }
        else
        {
            cpuScore = Smooth(cpuScore, runningProcess is null ? 55 : 76);
        }

        var rtt = TryReadNumberAfter(line, "RTT:");
        if (rtt is null)
        {
            rtt = TryReadNumberAfter(line, "rtt=");
        }
        if (rtt is { } rttValue)
        {
            networkScore = Smooth(networkScore, Math.Clamp(100 - rttValue, 15, 100));
        }
        else if (lower.Contains("latency") || lower.Contains("network") || lower.Contains("ping"))
        {
            networkScore = Smooth(networkScore, warningCount > 0 ? 62 : 84);
        }
        else
        {
            networkScore = Smooth(networkScore, runningProcess is null ? 70 : 78);
        }

        if (lower.Contains("migration"))
        {
            var migration = TryReadTrailingNumber(line) ?? 1;
            threadScore = Smooth(threadScore, Math.Clamp(92 - (migration * 12), 20, 100));
        }
        else if (lower.Contains("confirmed main tid") || lower.Contains("ema candidate"))
        {
            threadScore = Smooth(threadScore, 88);
        }
        else if (lower.Contains("thread") && lower.Contains("warn"))
        {
            threadScore = Smooth(threadScore, 58);
        }

        safetyScore = blockerCount > 0
            ? Smooth(safetyScore, 25)
            : warningCount > 0
                ? Smooth(safetyScore, Math.Max(55, 92 - warningCount * 8))
                : Smooth(safetyScore, 94);

        cpuTrend.Push(cpuScore);
        networkTrend.Push(networkScore);
        threadGauge.Value = Math.Clamp(threadScore, 0, 100);
        safetyGauge.Value = Math.Clamp(safetyScore, 0, 100);
        var healthScore = Math.Clamp((cpuScore + networkScore + threadScore + safetyScore) / 4, 0, 100);
        healthGauge.Value = healthScore;
        cpuMetric.Percent = Math.Clamp(cpuScore, 0, 100);
        threadMetric.Percent = Math.Clamp(threadScore, 0, 100);
        networkMetric.Percent = Math.Clamp(networkScore, 0, 100);
        cpuMetric.Detail = cpuMs.HasValue ? $"EMA CPU {cpuMs.GetValueOrDefault():0.0}ms" : "CPU/스레드 로그 기반";
        threadMetric.Detail = warningCount > 0 ? "경고 신호 관찰 중" : "메인 스레드 안정";
        networkMetric.Detail = rtt.HasValue ? $"RTT {rtt.GetValueOrDefault():0.0}ms" : "네트워크 로그 기반";
        threadGauge.Invalidate();
        safetyGauge.Invalidate();
        healthGauge.Invalidate();
        cpuMetric.Invalidate();
        threadMetric.Invalidate();
        networkMetric.Invalidate();
        heroTitleValue.Text = healthScore >= 85 ? "시스템이 안정적인 상태입니다" : healthScore >= 60 ? "최적화가 적용 중입니다" : "점검이 필요합니다";
        heroDescriptionValue.Text = blockerCount > 0
            ? "엔진 로그에서 차단 또는 오류 신호가 감지되었습니다. 상세 로그와 복구 상태를 확인하세요."
            : warningCount > 0
                ? "일부 경고가 감지되었습니다. 최적화는 유지되지만 상태를 관찰하는 것이 좋습니다."
                : "게임 실행 상태와 런타임 로그가 안정적으로 수집되고 있습니다.";
        visualSummaryValue.Text = $"샘플 {logSampleCount}개 · 경고 {warningCount}개 · 차단 {blockerCount}개";
    }

    private static double Smooth(double current, double next)
    {
        return (current * 0.7) + (next * 0.3);
    }

    private static double? TryReadNumberAfter(string line, string marker)
    {
        var index = line.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        if (index < 0)
        {
            return null;
        }

        var start = index + marker.Length;
        while (start < line.Length && char.IsWhiteSpace(line[start]))
        {
            ++start;
        }

        var end = start;
        while (end < line.Length && (char.IsDigit(line[end]) || line[end] == '.'))
        {
            ++end;
        }

        return end > start && double.TryParse(line[start..end], out var value) ? value : null;
    }

    private static double? TryReadTrailingNumber(string line)
    {
        var end = line.Length - 1;
        while (end >= 0 && !char.IsDigit(line[end]))
        {
            --end;
        }
        if (end < 0)
        {
            return null;
        }

        var start = end;
        while (start >= 0 && char.IsDigit(line[start]))
        {
            --start;
        }

        return double.TryParse(line[(start + 1)..(end + 1)], out var value) ? value : null;
    }

    private void UpdateSummaryState(string state, Color color, string optimizeText)
    {
        statusValue.Text = $"현재 상태 : {state}";
        statusValue.Font = DesignSystem.FontHeading;
        statusValue.ForeColor = color;
        optimizeStateValue.Text = optimizeText;
        recoveryStateValue.Text = "자동 복구 가능";
    }

    private void UpdateControlState(bool running)
    {
        startButton.Enabled = true;
        UpdatePrimaryButtonText(running);
        targetCombo.Enabled = !running;
        refreshButton.Enabled = !running;
        dryRunRadio.Enabled = !running;
        softApplyRadio.Enabled = !running;
        applyRadio.Enabled = !running;
    }

    private void UpdatePrimaryButtonText(bool? runningOverride = null)
    {
        var running = runningOverride ?? runningProcess is not null;
        startButton.Text = running ? "[종료]" : "[최적화]";
    }

    private void UpdateEnginePathLabel()
    {
        var path = FindEnginePath();
        enginePathValue.Text = path is null ? "엔진: 찾을 수 없음" : $"엔진: {path.FullName}";
        enginePathValue.ForeColor = path is null ? DesignSystem.Danger : DesignSystem.TextMuted;
    }

    private void UpdateApplyWarning()
    {
        UpdateModeDescription();
        if (applyRadio.Checked)
        {
            UpdateSummaryState("대기", DesignSystem.Warning, "실제 적용 준비");
        }
        else if (runningProcess is null)
        {
            statusValue.Text = "현재 상태 : 대기";
            statusValue.ForeColor = DesignSystem.TextMuted;
            recoveryStateValue.Text = "자동 복구 가능";
        }
    }

    private void BrowseBackgroundFilter()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "백그라운드 필터 파일 선택",
            Filter = "텍스트 파일 (*.txt)|*.txt|모든 파일 (*.*)|*.*",
            FileName = Path.GetFileName(backgroundFilterText.Text),
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            backgroundFilterText.Text = dialog.FileName;
            backgroundFilterCheck.Checked = true;
        }
    }

    private void OpenEvidenceFolder()
    {
        var folder = FindRepoScriptDirectory()?.FullName is { } scriptDir
            ? Path.Combine(scriptDir, "release_gate_logs")
            : Path.Combine(AppContext.BaseDirectory, "release_gate_logs");
        Directory.CreateDirectory(folder);
        Process.Start(new ProcessStartInfo { FileName = folder, UseShellExecute = true });
    }

    private void OpenLatestReport()
    {
        var evidence = FindRepoScriptDirectory()?.GetDirectories("release_gate_logs").FirstOrDefault();
        if (evidence is null || !evidence.Exists)
        {
            MessageBox.Show("아직 evidence report가 없습니다.", "리포트 없음", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }

        var selectedTarget = GetSelectedTargetName();
        var reports = evidence.GetFiles("rc_evidence_report.txt", SearchOption.AllDirectories)
            .OrderByDescending(file => file.LastWriteTimeUtc)
            .ToList();
        var report = string.IsNullOrWhiteSpace(selectedTarget)
            ? reports.FirstOrDefault()
            : reports.FirstOrDefault(file => ReportMatchesTarget(file, selectedTarget));
        if (report is null)
        {
            MessageBox.Show("선택한 대상의 rc_evidence_report.txt가 없습니다.", "리포트 없음", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        Process.Start(new ProcessStartInfo { FileName = report.FullName, UseShellExecute = true });
    }

    private static FileInfo? FindEnginePath()
    {
        foreach (var directory in WalkUp(new DirectoryInfo(AppContext.BaseDirectory)))
        {
            var candidates = new[]
            {
                Path.Combine(directory.FullName, "GameOptimizer.exe"),
                Path.Combine(directory.FullName, "x64", "Release", "GameOptimizer.exe"),
                Path.Combine(directory.FullName, "GameOptimizer", "GameOptimizer.exe"),
                Path.Combine(directory.FullName, "GameOptimizer", "x64", "Release", "GameOptimizer.exe"),
            };
            foreach (var candidate in candidates)
            {
                if (File.Exists(candidate))
                {
                    return new FileInfo(candidate);
                }
            }
        }
        return null;
    }

    private static bool ReportMatchesTarget(FileInfo report, string target)
    {
        try
        {
            foreach (var line in File.ReadLines(report.FullName).Take(32))
            {
                if (line.StartsWith("Target:", StringComparison.OrdinalIgnoreCase))
                {
                    var reportTarget = line["Target:".Length..].Trim();
                    return string.Equals(reportTarget, target, StringComparison.OrdinalIgnoreCase);
                }
            }
        }
        catch
        {
            return false;
        }

        return false;
    }

    private static DirectoryInfo? FindRepoScriptDirectory()
    {
        foreach (var directory in WalkUp(new DirectoryInfo(AppContext.BaseDirectory)))
        {
            var scriptDir = Path.Combine(directory.FullName, "GameOptimizer");
            if (File.Exists(Path.Combine(scriptDir, "run_rc_gate.bat")))
            {
                return new DirectoryInfo(scriptDir);
            }
            if (File.Exists(Path.Combine(directory.FullName, "run_rc_gate.bat")))
            {
                return directory;
            }
        }
        return null;
    }

    private static IEnumerable<DirectoryInfo> WalkUp(DirectoryInfo start)
    {
        for (var current = start; current is not null; current = current.Parent)
        {
            yield return current;
        }
    }

    private static string FindDefaultBackgroundFilter()
    {
        var scriptDir = FindRepoScriptDirectory();
        if (scriptDir is null)
        {
            return "background_filter_example.txt";
        }
        var path = Path.Combine(scriptDir.FullName, "background_filter_example.txt");
        return File.Exists(path) ? path : "background_filter_example.txt";
    }
}
