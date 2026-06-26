from ultralytics import YOLO # type: ignore

# Load your trained model
model = YOLO("D:/Model_vers.2/best.pt")

# Run prediction on all images in the folder
results = model.predict(
    source= "F:/test_images",
    conf=0.5,
    imgsz=640,
    save=True,
    show=False
)

print("Prediction complete!")
print("Results saved in runs/detect/predict/")