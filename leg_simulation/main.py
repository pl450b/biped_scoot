import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# Link lengths
l1, l2, l3, l4, l5 = 2, 2, 2, 2, 1.5

# User input
x_target = float(input("Enter X coordinate of end effector: "))
y_target = float(input("Enter Y coordinate of end effector: "))
ground_orientation = input("Enter ground beam orientation (in-in, in-out, out-in, out-out): ")

# Ground positions
ground1 = np.array([0, 0])
ground2 = np.array([4, 0])

def inverse_kinematics(x, y, orientation):
    """Compute the angles required to position the end effector at (x, y)."""
    d1 = np.linalg.norm([x, y] - ground1)
    d2 = np.linalg.norm([x, y] - ground2)
    
    if d1 > (l1 + l3) or d1 < abs(l1 - l3) or d2 > (l2 + l4) or d2 < abs(l2 - l4):
        return None, None  # No valid solution
    
    theta1 = np.arccos((l1**2 + d1**2 - l3**2) / (2 * l1 * d1))
    theta2 = np.arccos((l2**2 + d2**2 - l4**2) / (2 * l2 * d2))
    
    # Adjust angles based on orientation
    if orientation == "in-in":
        return theta1, theta2
    elif orientation == "in-out":
        return theta1, -theta2
    elif orientation == "out-in":
        return -theta1, theta2
    elif orientation == "out-out":
        return -theta1, -theta2
    return None, None

def forward_kinematics(theta1, theta2):
    """Compute the joint positions of the five-bar linkage."""
    A = ground1 + np.array([l1 * np.cos(theta1), l1 * np.sin(theta1)])
    B = ground2 + np.array([-l2 * np.cos(theta2), l2 * np.sin(theta2)])
    
    d = np.linalg.norm(A - B)
    if d > (l3 + l4) or d < abs(l3 - l4):
        return None, None, None  # No valid solution
    
    angle = np.arccos((l3**2 + d**2 - l4**2) / (2 * l3 * d))
    direction = (B - A) / d
    perp = np.array([-direction[1], direction[0]])
    
    P1 = A + l3 * (np.cos(angle) * direction + np.sin(angle) * perp)
    return A, P1, B

theta1, theta2 = inverse_kinematics(x_target, y_target, ground_orientation)
if theta1 is None or theta2 is None:
    print("No valid solution for the given input.")
    exit()

fig, ax = plt.subplots()
ax.set_xlim(-1, 5)
ax.set_ylim(-1, 3)
ax.set_aspect('equal')
ax.grid()

linkage, = ax.plot([], [], 'o-', lw=3)

def init():
    linkage.set_data([], [])
    return linkage,

def update(frame):
    A, P, B = forward_kinematics(theta1, theta2)
    if A is None:
        return linkage,
    
    x_data = [ground1[0], A[0], P[0], B[0], ground2[0]]
    y_data = [ground1[1], A[1], P[1], B[1], ground2[1]]
    linkage.set_data(x_data, y_data)
    return linkage,

ani = animation.FuncAnimation(fig, update, frames=1, init_func=init, blit=True)
plt.title(f"Angles: theta1={np.degrees(theta1):.2f}, theta2={np.degrees(theta2):.2f}")
plt.show()

