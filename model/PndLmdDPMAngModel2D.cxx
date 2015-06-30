/*
 * PndLmdDPMModel2D.cxx
 *
 *  Created on: Jan 17, 2013
 *      Author: steve
 */

#include "PndLmdDPMAngModel2D.h"

#include "TMath.h"
#include "TVector3.h"

PndLmdDPMAngModel2D::PndLmdDPMAngModel2D(std::string name_,
		shared_ptr<PndLmdDPMAngModel1D> dpm_model_1d_) :
		Model2D(name_), dpm_model_1d(dpm_model_1d_) {
	initModelParameters();
	this->addModelToList(dpm_model_1d);
	//getModelParameterSet().addModelParameters(
	//		dpm_model_1d.getModelParameterSet());
}

PndLmdDPMAngModel2D::~PndLmdDPMAngModel2D() {
	// TODO Auto-generated destructor stub
}

std::pair<double, double> PndLmdDPMAngModel2D::calculateThetaFromTiltedSystem(
		const double theta, const double phi) const {
	double x = tan(tilt_x->getValue());
	double y = tan(tilt_y->getValue());
	TVector3 tilt(x, y, 1.0);
	TVector3 rotate_axis(y, -x, 0.0);
	TVector3 measured_direction(sin(theta) * cos(phi), sin(theta) * sin(phi),
			cos(theta));
	measured_direction.Rotate(tilt.Theta(), rotate_axis);
	return std::make_pair(measured_direction.Theta(), measured_direction.Phi());
	/*
	 TVector3 tilt(tan(tilt_x->getValue()), tan(tilt_y->getValue()), 1.0);
	 TVector3 measured_direction(sin(theta) * cos(phi), sin(theta) * sin(phi),
	 cos(theta));

	 // determine euler angles for going from xyz to tilted frame
	 TVector3 zprime = tilt.Unit();
	 TVector3 yprime(-zprime.Y(), zprime.X(), 0.0);
	 TVector3 xprime = yprime.Cross(zprime);
	 double beta = acos(zprime.Z());
	 double alpha = atan2(zprime.X(), -zprime.Y());
	 double gamma = atan2(xprime.Z(), yprime.Z());

	 // this would be the transformation from xyz to tilted frame
	 //measured_direction.RotateZ(alpha);
	 //measured_direction.Rotate(beta, yprime);
	 //measured_direction.Rotate(gamma, zprime);
	 // this is the inverse
	 measured_direction.Rotate(-gamma, zprime);
	 measured_direction.Rotate(-beta, yprime);
	 measured_direction.RotateZ(-alpha);

	 return std::make_pair(measured_direction.Theta(), measured_direction.Phi());*/
}

double PndLmdDPMAngModel2D::calculateJacobianDeterminant(const double theta,
		const double phi) const {
	double e_m = 1.0 * 1e-16; // machine precision
	double ht = pow(e_m, 0.33) * theta;
	double hp = pow(e_m, 0.33) * phi;

	std::pair<double, double> shift_plus_ht = calculateThetaFromTiltedSystem(
			theta + ht, phi);
	std::pair<double, double> shift_min_ht = calculateThetaFromTiltedSystem(
			theta - ht, phi);
	std::pair<double, double> shift_plus_hp = calculateThetaFromTiltedSystem(
			theta, phi + hp);
	std::pair<double, double> shift_min_hp = calculateThetaFromTiltedSystem(theta,
			phi - hp);

	double j11 = (shift_plus_ht.first - shift_min_ht.first) / (2 * ht);
	double j12 = (shift_plus_hp.first - shift_min_hp.first) / (2 * hp);
	double j21 = (shift_plus_ht.second - shift_min_ht.second) / (2 * ht);
	double j22 = (shift_plus_hp.second - shift_min_hp.second) / (2 * hp);

	return j11 * j22 - j21 * j12;
}

void PndLmdDPMAngModel2D::initModelParameters() {
	tilt_x = getModelParameterSet().addModelParameter("tilt_x");
	tilt_y = getModelParameterSet().addModelParameter("tilt_y");
}

double PndLmdDPMAngModel2D::eval(const double *x) const {
	/*double theta_tilted(calculateThetaFromTiltedSystem(x[0], x[1]).first);
	double jaco(calculateJacobianDeterminant(x[0], x[1]));*/
/*	std::cout << "measured theta,phi: " << x[0] << "," << x[1]
			<< " -> transforms to evaluated theta of: " << theta_tilted << std::endl;
	std::cout << "jacobian: " << jaco << std::endl;*/
  /*if(theta_tilted > 0.001 && theta_tilted < 0.002) {
  	return jaco*1.0;
  }
  return 0.0;*/
	//return jaco * dpm_model_1d->eval(&theta_tilted) / (2.0 * TMath::Pi());
	return dpm_model_1d->eval(x) / (2.0 * TMath::Pi());
}

void PndLmdDPMAngModel2D::updateDomain() {
	setVar1Domain(0, TMath::Pi());
	setVar2Domain(-TMath::Pi(), TMath::Pi());
}
