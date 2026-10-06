using System.Collections;
using System.Numerics;
using Waffle;

// In-engine test suite: runs inside the player against Tests/TestProject and writes
// PASS/FAIL markers to the log. Scripts/RunTests.py asserts on those markers.
// Add new Expect() calls here when adding engine features with a script surface.
public class EngineTests : WaffleBehaviour
{
    private static int s_Passes = 0;
    private static int s_Failures = 0;

    private static void Expect(bool condition, string name)
    {
        if (condition)
        {
            s_Passes++;
            Log.Info($"TEST PASS: {name}");
        }
        else
        {
            s_Failures++;
            Log.Info($"TEST FAIL: {name}");
        }
    }

    private StateMachine ai;
    private bool nestedCoroutineDone;
    private bool triggerSeen;

    void OnStart()
    {
        StartCoroutine(RunAll());
    }

    private IEnumerator RunAll()
    {
        yield return null; // one frame: let spawns/physics settle before asserting

        // --- Scene queries -------------------------------------------------
        Entity camera = Scene.FindByName("Test Camera");
        Expect(camera.IsValid, "scene: FindByName");
        Entity emitter = Scene.FindByName("Particle Emitter");
        Expect(emitter.IsValid, "scene: particle emitter exists");

        // --- Particle system view ------------------------------------------
        if (emitter.IsValid)
        {
            ParticleSystem ps = emitter.Get<ParticleSystem>();
            ps.Emitting = true;
            yield return new WaitForSeconds(0.4f);
            int alive = ps.AliveCount;
            Expect(alive > 0, "particles: spawning raises alive count");

            ps.Burst(32);
            Expect(ps.AliveCount >= alive + 16, "particles: burst raises alive count");

            ps.Emitting = false;
            Expect(ps.Emitting == false, "particles: emitting toggles off");
            ps.Emitting = true;
        }
        else
        {
            Expect(false, "particles: emitter missing - view untestable");
        }

        // --- Steering -------------------------------------------------------
        Vector2 seek = Steering2D.Seek(Vector2.Zero, new Vector2(10.0f, 0.0f), 5.0f);
        Expect(seek.X == 5.0f && seek.Y == 0.0f, "steering: seek vector");

        // --- Blackboard -----------------------------------------------------
        Blackboard bb = new Blackboard();
        bb.Set("speed", 1.5f);
        bb.Set("name", "bot");
        bb.Set("home", new Vector2(1.0f, 2.0f));
        bb.Set("target", camera);
        Expect(bb.GetFloat("speed") == 1.5f
            && bb.GetString("name") == "bot"
            && bb.GetVector2("home").Y == 2.0f
            && bb.GetEntity("target") == camera,
            "blackboard: typed roundtrip");

        // --- State machine ---------------------------------------------------
        ai = new StateMachine(Entity);
        ai.AddState("Idle");
        ai.AddState("Chase");
        ai.AddState("Stun");
        ai.AddTransition("Idle", "Chase").After(0.2f);
        ai.AddGlobalTriggerTransition("Stun", "Hurt");
        ai.AddTransition("Stun", "Idle").After(0.2f);
        ai.Start("Idle");

        yield return new WaitForSeconds(0.5f);
        Expect(ai.IsIn("Chase"), "ai: timed transition fires");

        ai.Fire("Hurt");
        Expect(ai.IsIn("Stun"), "ai: global trigger fires");

        // Deterministic: resume the exact frame the recovery transition lands (Idle's own
        // timed transition to Chase fires 0.2s later, so a fixed sleep would race it).
        yield return new WaitUntil(() => ai.IsIn("Idle"));
        Expect(ai.IsIn("Idle") && ai.TimeInState < 0.2f, "ai: timed recovery from stun");

        // --- Coroutines -------------------------------------------------------
        StartCoroutine(NestedCoroutine());
        yield return new WaitForSeconds(0.3f);
        Expect(nestedCoroutineDone, "coroutine: nested + WaitForSeconds");

        int frames = 0;
        yield return new WaitUntil(() => { frames++; return frames >= 5; });
        Expect(frames >= 5, "coroutine: WaitUntil");

        // --- Physics -----------------------------------------------------------
        Entity box = Scene.FindByName("Physics Box");
        if (box.IsValid)
        {
            Rigidbody2D rb = box.Get<Rigidbody2D>();
            rb.Velocity = new Vector2(3.0f, 0.0f);
            yield return null;
            Expect(rb.Velocity.X > 2.0f, "physics: velocity roundtrip");
            rb.AddImpulse(new Vector2(0.0f, 1.0f));
            Expect(true, "physics: impulse applies without error");
        }
        else
        {
            Expect(false, "physics: box missing - rigidbody untestable");
        }

        // --- UI ------------------------------------------------------------------
        Entity text = Scene.FindByName("Test Text");
        if (text.IsValid)
        {
            UIText view = text.Get<UIText>();
            view.Text = "RUNNING";
            Expect(view.Text == "RUNNING", "ui: text set/get");
        }
        else
        {
            Expect(false, "ui: text entity missing");
        }

        // --- Transform ------------------------------------------------------------
        if (camera.IsValid)
        {
            Transform2D transform = camera.Get<Transform2D>();
            Vector3 original = transform.Position;
            transform.Position = new Vector3(5.0f, -3.0f, 0.5f);
            Vector3 read = transform.Position;
            transform.Position = original;
            Expect(read.X == 5.0f && read.Y == -3.0f && read.Z == 0.5f, "transform: position roundtrip");
        }

        // --- PersistentData ---------------------------------------------------------
        PersistentData.Set("runs", PersistentData.Get<int>("runs") + 1);
        Expect(PersistentData.Get<int>("runs") >= 1, "persistent data: survives between reads");

        // --- Summary ------------------------------------------------------------------
        Log.Info($"ENGINE TESTS: {s_Passes} passed, {s_Failures} failed");
        Log.Info(s_Failures == 0 ? "ENGINE TESTS RESULT: ALL PASS" : "ENGINE TESTS RESULT: FAILURES");

        yield return new WaitForSeconds(0.5f);
        Application.Quit();
    }

    private IEnumerator NestedCoroutine()
    {
        yield return new WaitForSeconds(0.1f);
        yield return InnerStep();
        nestedCoroutineDone = true;
    }

    private IEnumerator InnerStep()
    {
        yield return new WaitForSeconds(0.1f);
    }

    void OnDestroy()
    {
        Log.Info("TEST LIFECYCLE: OnDestroy ran");
    }
}
