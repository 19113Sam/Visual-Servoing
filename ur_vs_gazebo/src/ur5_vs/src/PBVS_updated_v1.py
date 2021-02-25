#!/usr/bin/env python2
import rospy
import numpy as np
import cv2
import jacobian_func
import Vcamera_to_Base
from cv_bridge import CvBridge, CvBridgeError
from sensor_msgs.msg import Image
from cv2 import aruco
import math
# from geometry_msgs import pose
# import jointstate msg type from shaunak repo
from std_msgs.msg import Header
from ur5_vs.msg import joint_angles
from ur5_vs.msg import joint_vel
from ur5_vs.msg import joint_states

# import jointvel msg type from shaunak repo
# import jointvel msg type from shaunak repo

bridge = CvBridge()

th1 = th2 = th3 = th4 = th5 = th6 = 0
th = [0, 0, 0, 0, 0, 0]

rgb_image = np.zeros([480, 640, 3], np.uint8)
# depth_img = np.zeros((480,640))
# depth_image = np.zeros([480, 640])\
Varm = np.array([0, 0, 0, 0, 0, 0])
th = np.array([0, 0, 0, 0, 0, 0])


def JS_callback(js):
    global th1, th2, th3, th4, th5, th6
    global th

    th1 = js.ang0.data
    th2 = js.ang1.data
    th3 = js.ang2.data
    th4 = js.ang3.data
    th5 = js.ang4.data
    th6 = js.ang5.data
    th = np.array([th1, th2, th3, th4, th5, th6])


def rgb_callback(rgb_msg):
    global rgb_image
    rgb_image = bridge.imgmsg_to_cv2(rgb_msg, 'bgr8')


# def desired_position_callback(desiredpose_Msg):
#     global desired_position
#     desired_position = np.asarray(desiredpose_Msg.data)
#     desired_position = np.reshape(desired_position,(7,1))
#     print('desired_position', desired_position)

# # Take position from the topic
# def current_position_callback(currentpose_Msg):
#     global current_position
#     current_position = np.asarray(currentpose_Msg.data)
#     current_position = np.reshape(current_position,(7,1))
#     print('current_position',current_position)


# def Object_detection():
    global key
    # global det
    global Varm
    global cameraMatrix
    global distCoeffs
    global current_position
    global desired_position

    cameraMatrix = np.array([[554.254691191187, 0.0, 320.5],
                             [0.0, 554.254691191187, 240.5], [0.0, 0.0, 1.0]])

    distCoeffs = np.array([0.0, 0.0, 0.0, 0.0, 0.0])

    # cameraMatrix = np.array([[607.4385986328125, 0.0, 315.3274841308594],
    # [0.0, 606.0167846679688, 241.0012969970703],[0.0, 0.0, 1.0]])

    # cameraMatrix = np.array([[525.0, 0.0, 319.5], [0.0, 525.0, 239.5],[0.0, 0.0, 1.0]])

    # cameraMatrix = np.array([[536.1547638382946, 0.0, 328.2989889116701],
    # [0.0, 534.2242890469014, 263.4929383043271],[0.0, 0.0, 1.0]])

    # distCoeffs= np.array([0.16587981496803042, -0.3142580999644871,
    # 0.0014280230760082854, 0.003129163390032306, 0.0])

    # distCoeffs= np.array([0.402, -0.497, 0.045, 0.019, 0.0])

    gray = cv2.cvtColor(rgb_image, cv2.COLOR_BGR2GRAY)
    aruco_dict = aruco.Dictionary_get(aruco.DICT_ARUCO_ORIGINAL)
    arucoParams = aruco.DetectorParameters_create()
    corners, ids, rejected_imgpoints = aruco.detectMarkers(gray, aruco_dict,
                                                           cameraMatrix, distCoeffs, parameters=arucoParams)

    # print(corners)

    if np.all(ids is not None):
        # global det  # if there are markers found by detector
        for i in range(0, len(ids)):

            # estimate pose of each marker and return the value of R_vec, T_vec ---
            # different from camera coefficeints

            R_vec, T_vec, _objPoints = aruco.estimatePoseSingleMarkers(corners[i],
                                                                       0.05, cameraMatrix, distCoeffs)

            # print("R_vec", R_vec)
            # print("T_vec", T_vec)

            # Function to Convert euler angle to axis angle rep
            def euler_yzx_to_axis_angle( z_e, x_e, y_e, normalize=True):
                # def euler_yzx_to_axis_angle(x_e, y_e, z_e, normalize=True):
                # Assuming the angles are in radians.
                c1 = math.cos(z_e/2)
                s1 = math.sin(z_e/2)
                c2 = math.cos(x_e/2)
                s2 = math.sin(x_e/2)
                c3 = math.cos(y_e/2)
                s3 = math.sin(y_e/2)
                c1c2 = c1*c2
                s1s2 = s1*s2
                w = c1c2*c3 - s1s2*s3
                x = c1c2*s3 + s1s2*c3
                y = s1*c2*c3 + c1*s2*s3
                z = c1*s2*c3 - s1*c2*s3
                angle = 2 * math.acos(w)
                if normalize:
                    norm = x*x+y*y+z*z
                    if norm < 0.001:
                        # when all euler angles are zero angle =0 so
                        # we can set axis to anything to avoid divide by zero
                        x = 1
                        y = 0
                        z = 0
                    else:
                        norm = math.sqrt(norm)
                        x /= norm
                        y /= norm
                        z /= norm
                return x, y, z, angle
                # return z, x, y, angle

            current_orr = (euler_yzx_to_axis_angle(R_vec.item(0), R_vec.item(1), R_vec.item(2)))
            current_position = []
            # current_position = np.append(T_vec, current_orr)

            #Current position with orientation in euler angles
            current_position = np.append(T_vec, R_vec)

            print("current position",current_position)

            # current_position = np.array([x, y,z, (euler_yzx_to_axis_angle(
            #     R_vec.item(0), R_vec.item(1), R_vec.item(2))]))
            # print(current_position), distortion_coefficients)

            c_x = (corners[i][0][0][0] + corners[i][0][1][0] +
                   corners[i][0][2][0] + corners[i][0][3][0]) / 4  # X coordinate of marker's center

            c_y = (corners[i][0][0][1] + corners[i][0][1][1] +
                   corners[i][0][2][1] + corners[i][0][3][1]) / 4  # Y coordinate of marker's center
            # print("c_x = ", c_x)
            # print("c_y = ", c_y)

            # get rid of numpy value array error
            (R_vec-T_vec).any()
            aruco.drawAxis(rgb_image, cameraMatrix,
                           distCoeffs, R_vec, T_vec, 0.1)

            # Draw a sqaure around the markers
        rgb_image = aruco.drawDetectedMarkers(rgb_image, corners)
        # cv2.imshow('image', det)


# def Visual_servoing(current_position, desired_position):
    # if joints_position[0] != 0:
        # if current_position[0, 0] != 0:

        # Desired Position
        R_vec_des = np.array(
            [3.14159265e+00,   5.96933344e-12,   5.10720960e-09])
        T_vec_des = np.array([-0.05625,  0.07784091,  0.62983489])
        Desired_orr = np.array(
            [1.00000000e+00, 2.55360480e-09, 2.98467236e-12, 3.14159265e+00])
        # desired pose in axis angle
        # desired_position = np.append(T_vec_des, Desired_orr)

        # Desired pose in euler angle and translation vector
        desired_position = np.append(T_vec_des, R_vec_des)
        print("desired position",desired_position)

        # Define error in position
        error = current_position - desired_position
        error = np.reshape(error, (6, 1))
        # error = np.reshape(error, (7, 1))
        # Mean error
        mean_error = math.sqrt(np.sum(np.square(error)))
        # print("error",error)
        # print("mean error",mean_error)

        # Interaction matrix
        I_3 = np.identity(3)

        skewvec = np.array([[0,  current_position[2], current_position[1]],
                            [current_position[2], 0, current_position[0]],
                            [current_position[1], current_position[0], 0]])
        u = np.append(current_orr,0)
        # print(u[1])

        th_u = u[3]
        # print(th_u)

        skew_u = np.array([[0, u[3], u[2]],
                           [u[3], 0, u[1]],
                           [u[2], u[1], 0]])

        L_th_u = I_3 - (th_u/2)*skew_u - (1-(np.sin(th_u)/np.sin((th_u/2)*(th_u/2))))*skew_u * skew_u

        # L_e = np.array([[I_3, skewvec],
        #             [np.zeros([3,3]), L_th_u]])
        L1 = np.concatenate([I_3, skewvec], axis=1)
        L2 = np.concatenate([np.zeros([3, 3]), L_th_u], axis=1)
        L_e = np.concatenate([L1, L2])

        # Inverse of Interaction matrix
        Inv_L_e = np.linalg.pinv(L_e)

        var_lambda = 1

        # CONTROL LAW to find Camera velocity
        Vcamera = -(var_lambda) * np.matmul(Inv_L_e, error)
        Vcamera[0] = -Vcamera[0]
        Vcamera[1] = -Vcamera[1]
        Vcamera[2] = -Vcamera[2]
        Vcamera[3] = Vcamera[3]
        Vcamera[4] = Vcamera[4]
        Vcamera[5] = Vcamera[5]
        print('Camera Velocity = ', Vcamera)

        # Convert Camera velocity into base frame
        V_base = Vcamera_to_Base.Vc_2_Base(Vcamera, th)

        # Finding jacobian
        jacobian = jacobian_func.calc_jack(th1, th2, th3, th4, th5, th6)

        # Joint space velocities
        Varm = np.matmul(jacobian, V_base)

        print("joint_velocity",Varm)


def main():
    global key
    global Varm
    global rgb_image
    # global det

    # Initialization node
    rospy.init_node('Aruco_detection', anonymous=True)

    # Subscribing
    rospy.Subscriber('my_joint_states', joint_states, JS_callback)

    # rospy.Subscriber('/camera/color/image_raw', Image, rgb_callback) ## REALSENSE
    rospy.Subscriber('/camera/rgb/image_raw', Image, rgb_callback, queue_size= 1)  # GAZEEBO AND REAL ROBOT
    pub = rospy.Publisher('joint_vel_cmd', joint_vel,  queue_size=10)
    jv = joint_vel()
    while not rospy.is_shutdown():
        jv.vel0.data = Varm.item(0)
        jv.vel1.data = Varm.item(1)
        jv.vel2.data = Varm.item(2)
        jv.vel3.data = Varm.item(3)
        jv.vel4.data = Varm.item(4)
        jv.vel5.data = Varm.item(5)
        print(jv)
        cv2.imshow('image', rgb_image)
        pub.publish(jv)

        key = cv2.waitKey(1)
        rospy.sleep(0.001)

        #     # Object_detection()
        #     if key == ord('q'):
        #         break
        # # KILL ALL WINDOWS
        # cv2.destroyAllWindows()


if __name__ == '__main__':
    try:
        main()
    except rospy.ROSInterruptException:
        pass
