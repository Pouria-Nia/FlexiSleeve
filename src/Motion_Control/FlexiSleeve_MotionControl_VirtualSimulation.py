import re
from py_openshowvar import openshowvar
import time
import websockets
import websocket
import json
import math
from threading import Thread, Lock

class MovingAverageFilter:
    def __init__(self, window_size):
        self.window_size = window_size
        self.buffer = []

    def filter(self, new_value):
        self.buffer.append(new_value)
        if len(self.buffer) > self.window_size:
            self.buffer.pop(0)
        return sum(self.buffer) / len(self.buffer)

data_lock = Lock()

in_roll = 0
in_pitch = 0
in_yaw = 0
switch = 0
btn_ctrl = 0

stop_flag = False
recv_flag = False



def connect_to_websocket():
    global in_roll, in_pitch, in_yaw, switch, btn_ctrl, stop_flag, recv_flag
    uri = "ws://192.168.1.22:81/"

    ws = websocket.WebSocket()
    ws.connect(uri)

    print("Connected to WebSocket")
    ws.send(json.dumps({"command": "read_sensor"}))

    while not stop_flag:
        try: 
            time.sleep(0.05)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
            message = ws.recv()
            data = json.loads(message)

            # Add more data fields as needed
            if data_lock.locked():
                continue

            data_lock.acquire()
            read_roll = data["roll"]
            #in_roll = (math.atan2(in_accY, in_accZ) * 180) / math.pi
            read_pitch = data["pitch"]
            #in_pitch = math.atan(-in_accX / math.sqrt(in_accY * in_accY + in_accZ * in_accZ)) * 180 / math.pi
            read_yaw = data["yaw"]

            switch = data["toggle_mode"] # 1: position mode / 2 : rotation mode / 3 : neutral mode
            btn_ctrl = data["button_hold_state"]
            recv_flag = True

            window_size = 3 
            maf_roll = MovingAverageFilter(window_size)
            maf_pitch = MovingAverageFilter(window_size)
            maf_yaw = MovingAverageFilter(window_size)

            # filtered_roll = []
            # filtered_pitch = []
            # filtered_yaw = []
            # Apply moving average filter to the data
            in_roll = maf_roll.filter(read_roll)
            in_pitch = maf_pitch.filter(read_pitch)
            in_yaw = maf_yaw.filter(read_yaw)

            # in_roll = {}.format(filtered_roll_value)
            # in_pitch = {}.format(filtered_pitch_value)
            # in_yaw = {}.format(filtered_yaw_value)

            # Store the filtered values
            # filtered_roll.append(filtered_roll_value)
            # filtered_pitch.append(filtered_pitch_value)
            # filtered_yaw.append(filtered_yaw_value)
            # Print the filtered values
            # print(f"Filtered Roll: {filtered_roll_value:.2f}, Filtered Pitch: {filtered_pitch_value:.2f}, Filtered Yaw: {filtered_yaw_value:.2f}")

            # Optional: Calculate and print the mean of filtered values
            # mean_roll = sum(filtered_roll) / len(filtered_roll)
            # mean_pitch = sum(filtered_pitch) / len(filtered_pitch)
            # mean_yaw = sum(filtered_yaw) / len(filtered_yaw)
            # print(f"Mean Roll: {mean_roll:.2f}, Mean Pitch: {mean_pitch:.2f}, Mean Yaw: {mean_yaw:.2f}")



            data_lock.release()

            #################################################################
        except :
            # client.close()
            print("WebSocket connection closed.")
            stop_flag = True
            break

    # with websockets.connect(uri) as websocket:
    #     print("Connected to WebSocket")

    #     # Send a message to trigger data sending from ESP32 if needed
    #     websocket.send(json.dumps({"command": "read_sensor"}))

    #     # Continuously receive and process messages from the WebSocket


        

    #     while not stop_flag:
    #         try:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         
    #             message = websocket.recv()
    #             data = json.loads(message)
    #             print("*******************************", '\n')

    #             # Add more data fields as needed
    #             if data_lock.locked():
    #                 continue

    #             data_lock.acquire()
    #             in_roll = data["roll"]
    #             #in_roll = (math.atan2(in_accY, in_accZ) * 180) / math.pi
    #             in_pitch = data["pitch"]
    #             #in_pitch = math.atan(-in_accX / math.sqrt(in_accY * in_accY + in_accZ * in_accZ)) * 180 / math.pi
    #             in_yaw = data["yaw"]

    #             switch = data["toggle_mode"] # 1: position mode / 2 : rotation mode / 3 : neutral mode
    #             btn_ctrl = data["button_hold_state"]
    #             recv_flag = True
    #             data_lock.release()

    #             #################################################################
    #         except websockets.exceptions.ConnectionClosed:
    #             # client.close()
    #             print("WebSocket connection closed.")
    #             stop_flag = True
    #             break

def main():
    global in_roll, in_pitch, in_yaw, switch, btn_ctrl, stop_flag, recv_flag
    client = openshowvar('192.168.153.128', 7000)
    print(client.can_connect)

    Thread(target = connect_to_websocket).start()

    while not stop_flag:
        try:
            time.sleep(1)
            if not recv_flag:
                continue
            # Extract e[ach variable from current position of the robot
            start = time.time()
            Pos_Act = client.read('$POS_ACT', debug=True)
            print(Pos_Act)
            # client.close()

            data_str = Pos_Act.decode('utf-8')

            # Use regular expression to extract values
            pattern = re.compile(r'(\w+) ([+-]?\d+\.\d+E?[+-]?\d*|[+-]?\d+\.?\d*)')
            matches = pattern.findall(data_str)

            # Convert the matches to a dictionary
            result = {key: float(value) for key, value in matches}

            # Print the result
            print(result)
            print("*" * 20)
            print(time.time() - start)
            print("*" * 20)

            Pos_X = result["X"]
            Pos_Y = result["Y"]
            Pos_Z = result["Z"]
            Pos_A = result["A"]
            Pos_B = result["B"]
            Pos_C = result["C"]

            # Take input from sensor data
            
            
            

            

            
            t = 0.5  # sec: estimated time for finishing one while loop

            out_speed = 0.25  # assigned speed
            swi_speed = 3

            send_flag = True
            data_lock.acquire()
            print("roll: {}, pitch: {}, yaw: {}".format(in_roll, in_pitch, in_yaw))
            print("toggle_mode: {}, btn: {}".format(switch, btn_ctrl))
            match switch: 
                case 1 : #go to the position mode
                    if max(abs(in_roll), abs(in_pitch)) < 15 or abs(abs(in_roll) - abs(in_pitch)) <= 10:
                        print("Skipping Mode_P due to condition")
                        send_flag = False #continue in while loop

                    elif abs(in_roll) > abs(in_pitch):
                        # if 20 >= abs(mean_roll) and abs(mean_roll) >= 10:
                        #     out_speed = 0.07
                        # elif 60 >= abs(mean_roll) and abs(mean_roll) > 20:
                        #     out_speed = 0.13
                        # else:
                        #     out_speed = 0.25

                        print("Going left/right")
                        Pos_Y += out_speed * 1000 * t * (in_roll/abs(in_roll))*(-1) # adjust direction

                        if Pos_Y >= 1000:
                            Pos_Y = 1000
                            print("Y axis out of safty range.")
                        elif Pos_Y <= -1000:
                            Pos_Y = -1000
                            print("Y axis out of safty range.")
                        


                    elif abs(in_roll) < abs(in_pitch):
                        
                        # if 20 >= abs(mean_pitch) and abs(mean_pitch) >= 10:
                        #     out_speed = 0.07
                        # elif 60 >= abs(mean_pitch) and abs(mean_pitch) > 20:
                        #     out_speed = 0.13
                        # else:
                        #     out_speed = 0.25

                        if btn_ctrl == False:
                            print("Going forward/backward")
                            Pos_X += out_speed * 1000 * t * (in_pitch/abs(in_pitch))*(-1) # adjust direction
                            if Pos_X >= 2000:
                                Pos_X = 2000
                                print("X axis out of safty range.")
                            elif Pos_X <= 1000:
                                Pos_X = 1000
                                print("X axis out of safty range.")
                            

                        elif btn_ctrl == True:
                            print("Going up/down")
                            Pos_Z += out_speed * 1000 * t * (in_pitch/abs(in_pitch)) # adjust direction
                            if Pos_Z >= 2000:
                                Pos_Z = 2000
                                print("Z axis out of safty range.")
                            elif Pos_Z <= 1300:
                                Pos_Z = 1300
                                print("Z axis out of safty range.")
                            

                case 2 : #go to the rotation mode       
                    if max(abs(in_roll), abs(in_pitch)) < 15 or abs(abs(in_roll) - abs(in_pitch)) <= 10 :
                        print("Skipping Mode_R due to condition")
                        send_flag = False #continue in while loop
                    elif abs(in_roll) > abs(in_pitch):
                        print("Rotating along Y direction")
                        Pos_C += swi_speed * t * (in_roll/abs(in_roll))
                    elif abs(in_roll) < abs(in_pitch):
                        if btn_ctrl == False:
                            print("Rotating along X direction")
                            Pos_B += swi_speed * t * (in_pitch/abs(in_pitch)) # adjust direction
                        elif btn_ctrl == True:
                            print("Rotating along X direction")
                            Pos_A += swi_speed * t * (in_pitch/abs(in_pitch)) # adjust direction
                case 3 :  #go to neutral mode
                    print("Skipping due to Mode_N or unknown mode")
                    send_flag = False
                case _:
                    send_flag = False
                    print("Invalid mode input.")

            data_lock.release()
            if not send_flag:
                continue
            print("Start send cmd")
            start = time.time()
            # speed = client.write('SPEED', "{:.3f}".format(out_speed) , debug = True)
            #ang_speed = client.write('SWI_SPEED', "{:.3f}".format(swi_speed) , debug = True)
            # time.sleep(0.3)
            time.sleep(0.3)

            basic_pos = "POS: X {:.2f}, Y {:.2f}, Z {:.2f}, A {:.2f}, B {:.2f}, C {:.2f}".format(Pos_X, Pos_Y, Pos_Z, Pos_A, Pos_B, Pos_C)
            pos = "{" + basic_pos + "}"
            update_pos = client.write('MyPosSimon', pos , debug = True)

            print("Finish")
            print(time.time() - start)

            # time.sleep(1)
            


            ##############################################################3

        except KeyboardInterrupt:
            stop_flag = True
            if data_lock.locked:
                data_lock.release()
            break
        

    client.close()


if __name__ == "__main__":
    main()



