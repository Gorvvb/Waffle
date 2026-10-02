namespace Waffle;

/// <summary>Base class for all Waffle gameplay scripts. Lifecycle methods are magic messages discovered by name - never write override: OnStart, OnUpdate/OnUpdate(float), OnDestroy, OnEnable/OnDisable, OnCollisionEnter2D/Exit2D, OnTriggerEnter2D/Exit2D, OnDrawGizmos (all optional, public or private instance methods). Constructors and field initializers must be side-effect-free: the editor instantiates the type to read inspector defaults.</summary>
public abstract class WaffleBehaviour
{
    /// <summary>The entity this instance is running on (bound by the engine).</summary>
    public Entity Entity { get; internal set; } = Entity.None;
}
