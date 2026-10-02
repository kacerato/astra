// Spike S-13 (ADR-D12b): custo e ganho do C# no aparelho, lado a lado com o Luau do S-08.
using System.Diagnostics;
using System.Reflection;
using System.Runtime.CompilerServices;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

static class Bench
{
    // Mesmo cálculo do S-08 (Luau): 20 milhões de iterações aritméticas.
    [MethodImpl(MethodImplOptions.NoInlining)]
    static double Aritmetica(int n)
    {
        double s = 0;
        for (int i = 1; i <= n; i++) s = s + i * 0.5 - i / 3.0 + Math.Floor(i / 7.0);
        return s;
    }

    // Mesmo cálculo do S-08: 20 mil passadas por um array de 1000 elementos.
    [MethodImpl(MethodImplOptions.NoInlining)]
    static double Arrays(int n)
    {
        var t = new double[1000];
        Array.Fill(t, 1.0);
        double s = 0;
        for (int r = 1; r <= n; r++)
            for (int i = 0; i < t.Length; i++) s += t[i] * r;
        return s;
    }

    static long RssKb()
    {
        try
        {
            foreach (var line in File.ReadLines("/proc/self/status"))
                if (line.StartsWith("VmRSS:")) return long.Parse(line.Split(' ', StringSplitOptions.RemoveEmptyEntries)[1]);
        }
        catch { }
        return -1;
    }

    static void Main(string[] args)
    {
        var sinceProcessStart = DateTime.Now - Process.GetCurrentProcess().StartTime;
        Console.WriteLine($"S13: .NET {Environment.Version}, {RuntimeInformationText()}, {IntPtr.Size * 8} bits");
        Console.WriteLine($"S13: Main alcançado {sinceProcessStart.TotalMilliseconds:F0} ms após o início do processo; RSS {RssKb() / 1024.0:F1} MB");

        // Aquecimento (o JIT compila na primeira chamada) e medição.
        Aritmetica(1000); Arrays(10);
        var sw = Stopwatch.StartNew();
        double a = Aritmetica(20_000_000);
        double aMs = sw.Elapsed.TotalMilliseconds;
        sw.Restart();
        double b = Arrays(20_000);
        double bMs = sw.Elapsed.TotalMilliseconds;
        Console.WriteLine($"S13: laços (20M aritmética + 20M acessos a array): {aMs + bMs:F1} ms (aritmética {aMs:F1}, arrays {bMs:F1}); resultado {a + b:G6}");

        // Chamada de método vazio (custo de chamada gerenciada, comparável à chamada Luau->Luau).
        sw.Restart();
        Action vazio = Vazio;
        for (int i = 0; i < 1_000_000; i++) vazio();
        Console.WriteLine($"S13: delegate vazio {sw.Elapsed.TotalMilliseconds * 1e6 / 1_000_000:F1} ns");

        // Roslyn: compilar um "script" de comportamento como o editor faria ao salvar.
        const string script = """
            public sealed class Porta
            {
                public float AnguloAbertura = 90f;
                public float Velocidade = 120f;
                float angulo;
                public void Update(float dt) { angulo = System.MathF.Min(AnguloAbertura, angulo + Velocidade * dt); }
                public float Angulo => angulo;
            }
            """;
        var refs = new[] { typeof(object).Assembly.Location, typeof(MathF).Assembly.Location,
                           Path.Combine(Path.GetDirectoryName(typeof(object).Assembly.Location)!, "System.Runtime.dll") }
                   .Distinct().Select(p => MetadataReference.CreateFromFile(p)).ToArray();
        for (int round = 1; round <= 3; round++)
        {
            sw.Restart();
            var compilation = CSharpCompilation.Create($"Script{round}", new[] { CSharpSyntaxTree.ParseText(script) }, refs,
                new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary, optimizationLevel: OptimizationLevel.Release));
            using var ms = new MemoryStream();
            var result = compilation.Emit(ms);
            double compileMs = sw.Elapsed.TotalMilliseconds;
            if (!result.Success)
            {
                foreach (var d in result.Diagnostics) Console.WriteLine($"S13: erro {d}");
                break;
            }
            var asm = Assembly.Load(ms.ToArray());
            dynamic porta = Activator.CreateInstance(asm.GetType("Porta")!)!;
            porta.Update(0.5f);
            Console.WriteLine($"S13: Roslyn compilação {round}: {compileMs:F0} ms ({ms.Length} B); Porta.Angulo após 0,5 s = {porta.Angulo}");
        }
        Console.WriteLine($"S13: RSS ao final {RssKb() / 1024.0:F1} MB");
    }

    static void Vazio() { }
    static string RuntimeInformationText() => System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription +
        " / " + System.Runtime.InteropServices.RuntimeInformation.RuntimeIdentifier;
}
