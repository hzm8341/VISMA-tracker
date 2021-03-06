// Dataset loader for Pix3d dataset.
#pragma once
// stl
#include <vector>
#include <array>

// 3rdparty
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "json/json.h"
#include "igl/readOBJ.h"
#include "glog/logging.h"

// feh
#include "alias.h"
#include "utils.h"
#include "message_utils.h"

namespace feh {

struct Pix3dPacket {
    Pix3dPacket(const std::string &dataroot, const Json::Value &record, bool has_edge=false):
      record_{record}
    {
        record_ = record;
        // load image & mask
        img_ = cv::imread(dataroot + record["img"].asString());
        mask_ = cv::imread(dataroot + record["mask"].asString(), CV_LOAD_IMAGE_GRAYSCALE);
        bbox_ = GetVectorFromJson<int, 4>(record, "bbox");

        // load pre-computed edge map
        if (has_edge) {
          std::string tmp = record["img"].asString();
          LoadEdgeMap(dataroot + std::string{tmp.begin(), tmp.end()-4} + ".edge", edge_);
        }

        // load object pose
        go_ = SE3{
            GetMatrixFromJson<ftype, 3, 3>(record, "rot_mat", JsonMatLayout::RowMajor),
            GetVectorFromJson<ftype, 3>(record, "trans_mat")};

        // camera intrinsics
        shape_ = GetVectorFromJson<int, 2>(record, "img_size");  // input layout: [width, height]
        std::swap(shape_[0], shape_[1]);    // shape layout: [height, width]
        focal_length_ = record["focal_length"].asDouble() / 32.0 * shape_[1];     // convert from mm to pixels
        K_ << focal_length_, 0, 0.5 * shape_[1],
              0, focal_length_, 0.5 * shape_[0],
              0,             0,               1;
        Kinv_ = K_.inverse();


        // construct camera pose from position and in-plane rotation
        cam_position_ = GetVectorFromJson<ftype, 3>(record, "cam_position");
        inplane_rotation_  = record["inplane_rotation"].asDouble();
        Vec3 dir = Vec3::Zero() - cam_position_;
        dir /= dir.norm();
        gc_ = SE3(SO3::exp(inplane_rotation_ * dir), cam_position_);

        // load CAD model
        LoadMesh(dataroot + record["model"].asString(), V_, F_);
    }
    cv::Mat img_, mask_, edge_;
    SE3 go_;  // object pose, applied to object directly
    SE3 gc_;  // camera pose FIXME: add more details later
    Vec3 cam_position_; // make the names consistent with pix3d json file
    ftype inplane_rotation_;    // assuming object sitting at the origin, camera is looking at the object center with an inplane rotation
    ftype focal_length_;
    Vec4i bbox_;
    MatX V_;    // vertices &
    MatXi F_;   // faces of CAD models
    Eigen::Matrix<int, 2, 1> shape_;  // shape of images: [rows, cols]
    Mat3 K_, Kinv_;
    Json::Value record_;

};

class Pix3dLoader {
public:
    Pix3dLoader(const std::string &dataroot): dataroot_(dataroot+"/") {
        // load json file

        // std::string json_path{dataroot + "/pix3d.json"};
        // FIXME: use the real json file
        std::string json_path{dataroot + "/pix3d.json"};
        std::cout << TermColor::cyan << "parsing json file ..." << TermColor::endl;
        json_ = LoadJson(json_path);
        // json_ = folly::parseJson(folly::json::stripComments(content));
    }

    /// \brief: Grab datum by indexing the json file.
    /// \param i: Index of the datum.
    Pix3dPacket GrabPacket(int i) {
        return {dataroot_, json_[i]};
    }

    /// \brief: Grab datum by filename of the datum.
    Pix3dPacket GrabPacket(const std::string &path) {
        for (int i = 0; i < json_.size(); ++i) {
            if (json_[i]["img"] == path) {
                return {dataroot_, json_[i]};
            }
        }
        throw std::out_of_range("failed to find datum at " + path);
    }

    int size() const {
      return json_.size();
    }



private:
    std::string dataroot_;
    // folly::dynamic json_;
    Json::Value json_;
};

} // namespace feh
