// Design Ref: §5 — WinForms entry point. (M1+M2 CLI harness moved to PowerShell test scripts.)
using IntegrateController.UI;

namespace IntegrateController;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }
}
