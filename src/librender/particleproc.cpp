/*
    This file is part of Mitsuba, a physically based rendering system.

    Copyright (c) 2007-2014 by Wenzel Jakob and others.

    Mitsuba is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Mitsuba is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <mitsuba/core/statistics.h>
#include <mitsuba/render/particleproc.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/range.h>

MTS_NAMESPACE_BEGIN

ParticleProcess::ParticleProcess(EMode mode, size_t workCount, size_t granularity,
        const std::string &progressText, const void *progressReporterPayload)
    : m_mode(mode), m_workCount(workCount), m_numGenerated(0),
      m_granularity(granularity), m_receivedResultCount(0) {

    /* Choose a suitable work unit granularity if none was specified */
    if (m_granularity == 0)
        m_granularity = std::max((size_t) 1, workCount /
            (16 * Scheduler::getInstance()->getWorkerCount()));

    /* Create a visual progress reporter */
    m_progress = new ProgressReporter(progressText, workCount,
        progressReporterPayload);
    m_resultMutex = new Mutex();
}

ParticleProcess::~ParticleProcess() {
    delete m_progress;
}

ParallelProcess::EStatus ParticleProcess::generateWork(WorkUnit *unit, int worker) {
    RangeWorkUnit *range = static_cast<RangeWorkUnit *>(unit);
    size_t workUnitSize;

    if (m_mode == ETrace) {
        if (m_numGenerated == m_workCount)
            return EFailure; // There is no more work

        workUnitSize = std::min(m_granularity, m_workCount - m_numGenerated);
    } else {
        if (m_receivedResultCount >= m_workCount)
            return EFailure; // There is no more work

        workUnitSize = m_granularity;
    }

    range->setRange(m_numGenerated, m_numGenerated + workUnitSize - 1);
    m_numGenerated += workUnitSize;

    return ESuccess;
}

void ParticleProcess::increaseResultCount(size_t resultCount) {
    LockGuard lock(m_resultMutex);
    m_receivedResultCount += resultCount;
    m_progress->update(m_receivedResultCount);
}

ParticleTracer::ParticleTracer(int maxDepth, int rrDepth, bool emissionEvents)
    : m_maxDepth(maxDepth), m_rrDepth(rrDepth), m_emissionEvents(emissionEvents) { }

ParticleTracer::ParticleTracer(Stream *stream, InstanceManager *manager)
    : WorkProcessor(stream, manager) {

    m_maxDepth = stream->readInt();
    m_rrDepth = stream->readInt();
    m_emissionEvents = stream->readBool();
}

void ParticleTracer::serialize(Stream *stream, InstanceManager *manager) const {
    stream->writeInt(m_maxDepth);
    stream->writeInt(m_rrDepth);
    stream->writeBool(m_emissionEvents);
}

ref<WorkUnit> ParticleTracer::createWorkUnit() const {
    return new RangeWorkUnit();
}

void ParticleTracer::prepare() {
    Scene *scene = static_cast<Scene *>(getResource("scene"));
    m_scene = new Scene(scene);
    m_sampler = static_cast<Sampler *>(getResource("sampler"));
    Sensor *newSensor = static_cast<Sensor *>(getResource("sensor"));
    m_scene->removeSensor(scene->getSensor());
    m_scene->addSensor(newSensor);
    m_scene->setSensor(newSensor);
    m_scene->initializeBidirectional();
}

void ParticleTracer::process(const WorkUnit *workUnit, WorkResult *workResult,
        const bool &stop) {
    const RangeWorkUnit *range = static_cast<const RangeWorkUnit *>(workUnit); //准备工作
    MediumSamplingRecord mRec;
    Intersection its;
    ref<Sensor> sensor    = m_scene->getSensor();
    bool needsTimeSample  = sensor->needsTimeSample();
    PositionSamplingRecord pRec(sensor->getShutterOpen()
        + 0.5f * sensor->getShutterOpenTime());
    Ray ray;

    m_sampler->generate(Point2i(0));
    //对每个光子循环
    for (size_t index = range->getRangeStart(); index <= range->getRangeEnd() && !stop; ++index) {
        m_sampler->setSampleIndex(index);

        /* Sample一个光子 */
        if (needsTimeSample)
            pRec.time = sensor->sampleTime(m_sampler->next1D());

        const Emitter *emitter = NULL;
        const Medium *medium;

        Spectrum power;
        Ray ray;

        //储存pdf等变量的的vector
        std::vector<Float> vecPdf;
        std::vector<Float> vecInvPdf;
        std::vector<Spectrum> vecInvEval;

        if (m_emissionEvents) {
            /* 分别sample光子的位置和方向 */
            power = m_scene->sampleEmitterPosition(pRec, m_sampler->next2D());
            emitter = static_cast<const Emitter *>(pRec.object);
            medium = emitter->getMedium();

            /* 与handle绑定 */
            handleEmission(pRec, medium, power);

            DirectionSamplingRecord dRec;
            power *= emitter->sampleDirection(dRec, pRec,
                    emitter->needsDirectionSample() ? m_sampler->next2D() : Point2(0.5f));
            ray.setTime(pRec.time);
            ray.setOrigin(pRec.p);
            ray.setDirection(dRec.d);
        } else {
            /* 同时sample位置和方向 */

            Float emPdf; //增加

            power = m_scene->sampleEmitterRay(ray, emitter,
                m_sampler->next2D(), m_sampler->next2D(), 
                emPdf, //增加
                pRec.time);
            medium = emitter->getMedium();

            //vecPdf.push_back(emPdf); 

            handleNewParticle();
        }

        int depth = 1, nullInteractions = 0;
        bool delta = false;
        int fakedepth = 0;
        Spectrum throughput(1.0f); // throughput归一,由于russianroulette
        while (!throughput.isZero() && (depth <= m_maxDepth || m_maxDepth < 0)) {
            Float tmpPdf,tmpInvPdf;
            Spectrum tmpInvEval;
            m_scene->rayIntersectAll(ray, its);

            /* ==================================================================== */
            /*                 Radiative Transfer Equation sampling                 */
            /* ==================================================================== */
            if(medium)    Log(EInfo, "medium");
            if(medium && medium->sampleDistance(Ray(ray, 0, its.t), mRec, m_sampler)) {
                /* Sample the integral
                  \int_x^y tau(x, x') [ \sigma_s \int_{S^2} \rho(\omega,\omega') L(x,\omega') d\omega' ] dx'
                */
                throughput *= mRec.sigmaS * mRec.transmittance / mRec.pdfSuccess;
                tmpPdf = 1.0/mRec.pdfSuccess;
                handleMediumInteraction(depth, nullInteractions,
                        delta, mRec, medium, -ray.d, throughput*power, vecPdf); //待修改

                PhaseFunctionSamplingRecord pRec(mRec, -ray.d, EImportance);

                throughput *= medium->getPhaseFunction()->sample(pRec, m_sampler);
                delta = false;

                ray = Ray(mRec.p, pRec.wo, ray.time);
                ray.mint = 0;
            } else if (its.t == std::numeric_limits<Float>::infinity()) {
                /* 如果这个方向没有表面 */
                break;
            } else {
                /* 当发射反射的时候 */
                if (medium)
                    throughput *= mRec.transmittance / mRec.pdfFailure;

                const BSDF *bsdf = its.getBSDF();

                //进行sample
                BSDFSamplingRecord bRec(its, m_sampler, EImportance);
                Spectrum bsdfWeight = bsdf->sample(bRec, m_sampler->next2D());
                handleSurfaceInteraction(depth, nullInteractions, delta, its, medium, throughput*power, vecPdf, vecInvPdf, vecInvEval, throughput,bRec.wi); //已移位

                if (bsdfWeight.isZero())
                    break;

                 //改变hitpoint的入射出射角,计算对应的pdf和eval,用于计算重要性采样权重
                tmpPdf = bsdf->pdf(bRec,bRec.sampledType ==BSDF:: EDeltaReflection?EDiscrete:ESolidAngle);
                BSDFSamplingRecord tmpbRec = bRec;
                tmpbRec.wi = bRec.wo;
                tmpbRec.wo = bRec.wi;  
                tmpInvPdf = bsdf->pdf(tmpbRec,tmpbRec.sampledType ==BSDF:: EDeltaReflection?EDiscrete:ESolidAngle);
                tmpInvEval = bsdf->eval(tmpbRec,tmpbRec.sampledType ==BSDF:: EDeltaReflection?EDiscrete:ESolidAngle);


                /* 用 shading normals防止光泄露 */
                Vector wi = -ray.d, wo = its.toWorld(bRec.wo);
                Float wiDotGeoN = dot(its.geoFrame.n, wi),
                      woDotGeoN = dot(its.geoFrame.n, wo);
                if (wiDotGeoN * Frame::cosTheta(bRec.wi) <= 0 ||
                    woDotGeoN * Frame::cosTheta(bRec.wo) <= 0)
                    break;

                /* 更新eta和throughput */

                throughput *= bsdfWeight;
                if (its.isMediumTransition())
                    medium = its.getTargetMedium(woDotGeoN);

                if (bRec.sampledType & BSDF::ENull)
                    ++nullInteractions;
                else
                    delta = bRec.sampledType & BSDF::EDelta;

#if 0
                /* This is somewhat unfortunate: for accuracy, we'd really want the
                   correction factor below to match the path tracing interpretation
                   of a scene with shading normals. However, this factor can become
                   extremely large, which adds unacceptable variance to output
                   renderings.

                   So for now, it is disabled. The adjoint particle tracer and the
                   photon mapping variants still use this factor for the last
                   bounce -- just not for the intermediate ones, which introduces
                   a small (though in practice not noticeable) amount of error. This
                   is also what the implementation of SPPM by Toshiya Hachisuka does.

                   Ultimately, we'll need better adjoint BSDF sampling strategies
                   that incorporate these extra terms */

                /* Adjoint BSDF for shading normals -- [Veach, p. 155] */
                throughput *= std::abs(
                    (Frame::cosTheta(bRec.wi) * woDotGeoN)/
                    (Frame::cosTheta(bRec.wo) * wiDotGeoN));
#endif
            //更新ray的方向
                ray.setOrigin(its.p);
                ray.setDirection(wo);
                ray.mint = Epsilon;
            }

            if (depth++ >= m_rrDepth) {
                 /* Russian roulette技巧,一定概率结束光线,用于提高效率 */
                
                Float q = std::min(throughput.max(), (Float) 0.95f);
                if (m_sampler->next1D() >= q)
                    break;
                throughput /= q;
                vecPdf.push_back(tmpPdf*q); 
            }
            else
                vecPdf.push_back(tmpPdf); /* 更新对应的vector */
                vecInvPdf.push_back(tmpInvPdf);
                vecInvEval.push_back(tmpInvEval);
        }
    }
}

void ParticleTracer::handleEmission(const PositionSamplingRecord &pRec,
        const Medium *medium, const Spectrum &weight) { }

void ParticleTracer::handleNewParticle() { }

void ParticleTracer::handleSurfaceInteraction(int depth, int nullInteractions,
    bool delta, const Intersection &its, const Medium *medium,
    const Spectrum &weight, const std::vector<Float> & vecPdf, const std::vector<Float> & vecInvPdf, 
    const std::vector<Spectrum> & vecInvEval, Spectrum throughput,const Vector & wi) { }

void ParticleTracer::handleMediumInteraction(int depth, int nullInteractions,
    bool delta, const MediumSamplingRecord &mRec, const Medium *medium,
    const Vector &wi, const Spectrum &weight, const std::vector<Float> & vecPdf) { }

MTS_IMPLEMENT_CLASS(RangeWorkUnit, false, WorkUnit)
MTS_IMPLEMENT_CLASS(ParticleProcess, true, ParallelProcess)
MTS_IMPLEMENT_CLASS(ParticleTracer, true, WorkProcessor)
MTS_NAMESPACE_END
