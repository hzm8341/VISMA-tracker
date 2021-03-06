#include <fstream>
#include <iostream>

#include "glog/logging.h"

#include "absl/strings/str_format.h"
#include "json/json.h"

// feh
#include "tracker.h"
#include "tracker_utils.h"
#include "scene.h"
#include "dataloaders.h"
#include "utils.h"

using namespace feh;

using TermColor = feh::TermColor;

int main(int argc, char **argv) {
    std::string config_file("../cfg/multiple_object_tracking.json");

    auto config = LoadJson(config_file);

    std::string dataset_root(config["dataset_root"].asString() + "/");

    std::string dataset;
    if (argc == 1) {
        dataset = config["dataset"].asString();
    } else {
        dataset = std::string(argv[1]);
    }
    dataset_root += dataset;
    int wait_time(0);
    wait_time = config["wait_time"].asInt();


    std::shared_ptr<feh::VlslamDatasetLoader> loader;
    bool is_sceneNN(false);
    if (config["datatype"].asString() == "VLSLAM") {
        std::cout << dataset_root << "\n";
        loader = std::make_shared<feh::VlslamDatasetLoader>(dataset_root);
    } else if (config["datatype"].asString() == "ICL") {
        std::cout << dataset_root << "\n";
        loader = std::make_shared<feh::ICLDatasetLoader>(dataset_root);
    } else if (config["datatype"].asString() == "SceneNN") {
        std::cout << dataset_root << "\n";
        loader = std::make_shared<feh::SceneNNDatasetLoader>(dataset_root);
        is_sceneNN = true;
    } else if (config["datatype"].asString() == "KITTI") {
        std::cout << dataset_root << "\n";
        loader = std::make_shared<feh::KittiDatasetLoader>(dataset_root);
    }

    feh::tracker::Scene scene;
    scene.Initialize(config["scene_config"].asString(), config);

    // create windows
    int start_index(config["start_index"].asInt());
    char *dump_dir = nullptr;

    if (config["save"].asBool()) {
        char temp_template[256];
        sprintf(temp_template, "%s_XXXXXX", dataset.c_str());
        dump_dir = mkdtemp(temp_template);
    }


    for (int i = 0; i < loader->size(); ++i) {
        std::cout << "outer loop " <<  i << "/" << loader->size() << "\n";
        cv::Mat img, edgemap;
        vlslam_pb::BoundingBoxList bboxlist;
        SE3 gwc;
        SO3 Rg;

        std::string imagepath;
        bool succeed = loader->Grab(i, img, edgemap, bboxlist, gwc, Rg, imagepath);
        std::cout << TermColor::red + TermColor::bold << "BOX NUMBER=" << bboxlist.bounding_boxes_size() << TermColor::endl;
        if (!succeed) break;
        std::cout << imagepath << "\n";
        if (i == 0) {
            // global reference frame
            if (is_sceneNN) {
                scene.SetInitCameraToWorld(SE3{});
            } else {
                scene.SetInitCameraToWorld(gwc);
            }
        } else if (i < start_index) {
            continue;
        }

        scene.Update(edgemap, bboxlist, gwc, Rg, img, imagepath);
//        scene.Update(edgemap, pruned_bboxlist, gwc, Rg, img);
        const auto &display = scene.Get2DView();
        const auto &zbuffer = scene.GetZBuffer();
        const auto &segmask = scene.GetSegMask();

        cv::imshow("tracker view", display);
//        cv::imshow("depth buffer", zbuffer);
//        cv::imshow("segmentation mask", segmask);

        if (dump_dir != nullptr) {
            cv::imwrite(absl::StrFormat("%d/%0.6d.png", dump_dir, i),
                        display);
        }

        char ckey = cv::waitKey(wait_time);
        if (ckey == 'q') {
            break;
        } else if (ckey == 'p') {
            // pause
            wait_time = 0;
        } else if (ckey == 'r') {
            wait_time = config["wait_time"].asInt();
        } else if (ckey == 's') {
            if (dump_dir != nullptr) {
                scene.WriteLogToFile(absl::StrFormat("%d/result_%04d.json", dump_dir, i));
            }
        }
    }
    if (dump_dir != nullptr) {
        scene.WriteLogToFile(absl::StrFormat("%d/result.json", dump_dir));
    }
}
