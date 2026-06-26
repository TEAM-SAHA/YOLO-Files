from ultralytics import YOLO
import cv2
import threading
import time

model = YOLO(r"F:\best.pt")

STREAM_URL = "http://10.159.245.136:4747/video"

latest_frame = None
running = True
    
def camera_reader():
    global latest_frame

    cap = cv2.VideoCapture(STREAM_URL)

    while running:
        ret, frame = cap.read()
        if ret:
            latest_frame = frame

    cap.release()

threading.Thread(target=camera_reader, daemon=True).start()

while True:

    if latest_frame is None:
        continue

    frame = latest_frame.copy()

    # Reduce size for faster processing
    frame = cv2.resize(frame, (320, 240))
    results = model(frame)
    annotated = results[0].plot()

    cv2.imshow("Crack Detection", annotated)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        running = False
        break

cv2.destroyAllWindows()