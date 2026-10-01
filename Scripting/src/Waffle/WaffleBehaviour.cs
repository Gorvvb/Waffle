namespace Waffle;

/// <summary>
/// Base class for all Waffle gameplay scripts.
/// </summary>
/// <remarks>
/// Lifecycle methods are <b>magic messages</b>: declare them as plain methods
/// and the engine discovers them by name - never write <c>override</c>.
/// Current message set (all optional, public or private, instance methods):
///
///   void OnStart()                      - entity created / play started
///   void OnUpdate() / OnUpdate(float)   - per frame (dt = seconds)
///   void OnDestroy()                    - entity destroyed / play stopped
///   void OnEnable() / void OnDisable()  - SetActive transitions
///   void OnCollisionEnter2D(Entity other) / void OnCollisionExit2D(Entity other)
///   void OnTriggerEnter2D(Entity other)  / void OnTriggerEnter2D(Entity other)
///   void OnDrawGizmos()                 - editor-only debug drawing
///
/// Constructors and field initializers must be side-effect-free: the editor
/// instantiates the type to read default field values for the inspector.
/// </remarks>
public abstract class WaffleBehaviour
{
    /// <summary>The entity this instance is running on (bound by the engine).</summary>
    public Entity Entity { get; internal set; } = Entity.None;
}
