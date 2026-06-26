# too much buffering

from ultralytics import YOLO
import cv2

model = YOLO(r"F:\best.pt")

url = "http://10.159.245.136:4747/video"

cap = cv2.VideoCapture(url)

while True:
    ret, frame = cap.read()
    if not ret:
        continue

    results = model(frame)

    annotated = results[0].plot()

    cv2.imshow("Crack Detection", annotated)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()