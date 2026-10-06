using System.Collections;

namespace Waffle;

/// <summary>Base class for all Waffle gameplay scripts. Lifecycle methods are magic messages discovered by name - never write override: OnStart, OnUpdate/OnUpdate(float), OnDestroy, OnEnable/OnDisable, OnCollisionEnter2D/Exit2D, OnTriggerEnter2D/Exit2D, OnDrawGizmos (all optional, public or private instance methods). Constructors and field initializers must be side-effect-free: the editor instantiates the type to read inspector defaults.</summary>
public abstract class WaffleBehaviour
{
    /// <summary>The entity this instance is running on (bound by the engine).</summary>
    public Entity Entity { get; internal set; } = Entity.None;

    /// <summary>Starts a coroutine owned by this script's entity; yield WaitForSeconds/WaitForFrames/WaitUntil/WaitWhile or another IEnumerator. Stops with the entity, the scene, or StopCoroutine.</summary>
    protected CoroutineHandle StartCoroutine(IEnumerator routine) => CoroutineSystem.Start(Entity, routine);

    /// <summary>Cancels a coroutine started by this script.</summary>
    protected void StopCoroutine(CoroutineHandle handle) => handle.Stop();

    /// <summary>Cancels every coroutine owned by this script's entity.</summary>
    protected void StopAllCoroutines() => CoroutineSystem.StopAllForEntity(Entity.Id);
}
