from ultralytics import YOLO
import cv2

model = YOLO(r"F:\best.pt")

STREAM_URL = "http://10.159.245.136:4747/video"

cap = cv2.VideoCapture(STREAM_URL)

frame_count = 0

while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame_count += 1

    # Process only every 3rd frame
    if frame_count % 3 != 0:
        continue

    # Smaller image = faster
    frame = cv2.resize(frame, (320, 240))

    results = model(
        frame,
        imgsz=320,
        conf=0.5,
        verbose=False
    )

    annotated = results[0].plot()

    cv2.imshow("Crack Detection", annotated)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()