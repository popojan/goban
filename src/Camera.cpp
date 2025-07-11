#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;

#include "Camera.h"

glm::dmat4x4 getMatrix(const DDG::Quaternion& r) {
    auto w = (float)r[0];
    auto x = (float)r[1];
    auto y = (float)r[2];
    auto z = (float)r[3];
    return  {
        1.f-2.f*y*y-2.f*z*z, 2.f*x*y+2.f*w*z, 2.f*x*z-2.f*w*y, 0.f,
        2.f*x*y-2.f*w*z, 1.f-2.f*x*x-2.f*z*z, 2.f*y*z+2.f*w*x, 0.f,
        2.f*x*z+2.f*w*y, 2.f*y*z-2.f*w*x, 1.f-2.f*x*x-2.f*y*y, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
}

static DDG::Quaternion toQuaternion(double pitch, double roll, double yaw)
{
	double t0 = std::cos(yaw * 0.5);
	double t1 = std::sin(yaw * 0.5);
	double t2 = std::cos(roll * 0.5);
	double t3 = std::sin(roll * 0.5);
	double t4 = std::cos(pitch * 0.5);
	double t5 = std::sin(pitch * 0.5);

	DDG::Quaternion q(
	t0 * t2 * t4 + t1 * t3 * t5,
	t0 * t3 * t4 - t1 * t2 * t5,
	t0 * t2 * t5 + t1 * t3 * t4,
	t1 * t2 * t4 - t0 * t3 * t5);
	return q;
}

namespace DDG {
    Camera::Camera(float x, float y, float z, float w)
            : pClick(1.),
              pDrag(1.),
              pLast(1.),
              rLast(x, y, z, w),
              momentum(1.),
              tLast(0),
              zoom(1.),
              vZoom(1.),
              axis(Y_AXIS),
              lock(true) {}

    void Camera::setHorizontalLock(bool newLock) {
        lock = newLock;
    }

    Quaternion Camera::clickToSphere(float x, float y, float w, float h) {

        float alphaW = 2.f * x / w - 1.f;
        float alphaH = 2.f * y / h - 1.f;

        Quaternion p(0.0, -alphaW, alphaH, 0.0);

        if (p.norm2() > 1.) {
            p.normalize();
            p.im().z = 0.;
        } else {
            p.im().z = sqrt(1. - p.norm2());
        }
        return p;
    }

    Quaternion Camera::currentRotation() const {
        return pDrag * pClick.conj() * rLast;
    }

    glm::dmat4 Camera::getView(bool reverse) const {
        return getMatrix(reverse ? rLast.conj() : rLast);
    }


    glm::dmat4 Camera::setView() const {
        return getMatrix(currentRotation().conj());
    }

    void Camera::mouse(int state, float x, float y, float w, float h) {
        if (state == 1) {
            double a = std::tan(3.1415 / 4);
            double b = (y - h / 2) / (x - w / 2);
            double c = std::tan(3.1415 / 2 - 3.1415 / 4);
            if (lock && (b < a && b > -a)) {
                axis = X_AXIS;
                pClick = pDrag = DDG::Quaternion(1.0);
                pLast = DDG::Quaternion(x, 0.0, 0.0, 0.0);
            } else if (lock && (b > c || b < -c)) {
                axis = Y_AXIS;
                pClick = pDrag = pLast = clickToSphere(w / 2, y, w, h);
            } else {
                pClick = pDrag = pLast = clickToSphere(x, y, w, h);
            }
            momentum = 1.;
        }
        if (state == 0) {
            momentum = 1.;
            rLast = currentRotation();
            pClick = pDrag = 1.;
        }
    }

    void Camera::motion(float x, float y, float w, float h) {
        if (axis == Y_AXIS) {
            pDrag = clickToSphere(w / 2, y, w, h);
        } else if (axis == X_AXIS) {
            pDrag = (rLast * toQuaternion(3 * (x - pLast.re()) / w, 0, 0) * rLast.conj()).conj();//
        } else {
            pDrag = clickToSphere(x, y, w, h);
        }
    }

}